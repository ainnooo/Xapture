#include "generator.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "common/define.h"
#include "common/xdp_base.h"

namespace {
// Convert enum class Action to String.
Action ConvertActionFromString(const std::string& action) {
  if (action == "pass") {
    return Action::Pass;
  } else if (action == "drop") {
    return Action::Drop;
  } else {
    std::cout << "action must be 'pass' or 'drop'" << std::endl;
    exit(EXIT_FAIL);
  }
}

// Convert decimal int to hex string.
std::string ConvertDecimalIntToHexString(int dec) {
  if (!dec) {
    return std::string("0");
  }
  std::string hex;
  const char hc = 'a';
  while (dec != 0) {
    int d = dec & 15;
    if (d < 10) {
      hex.insert(hex.begin(), d + '0');
    } else {
      hex.insert(hex.begin(), d - 10 + hc);
    }
    dec >>= 4;
  }
  if (hex.length() == 1) {
    return "0" + hex;
  } else {
    return hex;
  }
}

// Convert String ip address to hex string.
std::string ConvertIPAddressToHexString(std::string& address) {
  std::string::size_type pos;
  std::string splitter = ".";
  std::string subpart;
  std::string hex;
  for (int i = 0; i < 4; i++) {
    pos = address.find(splitter);
    subpart = address.substr(0, pos);
    hex = ConvertDecimalIntToHexString(stoi(subpart)) + hex;
    address.erase(0, pos + splitter.size());
  }
  return "0x" + hex;
};
}  // namespace

Generator::Generator(const std::string& yaml_filepath)
    : yaml_filepath_(yaml_filepath) {
  ReadYaml();
}

Generator::~Generator() {
  std::cout << "Generator destructor" << std::endl;
}

void Generator::ReadYaml() {
  const YAML::Node& yaml_policies = YAML::LoadFile(yaml_filepath_);
  int priority = 0;
  for (const auto& yaml_policy : yaml_policies) {
    Policy policy;
    policy.priority = priority;
    if (!yaml_policy["action"]) {
      std::cout << "rule must have action value." << std::endl;
    }
    policy.action =
        ConvertActionFromString(yaml_policy["action"].as<std::string>());
    if (yaml_policy["port"]) {
      policy.port = yaml_policy["port"].as<int>();
    }
    if (yaml_policy["ip_address"]) {
      policy.ip_address = yaml_policy["ip_address"].as<std::string>();
    }
    if (yaml_policy["protocol"]) {
      policy.protocol = yaml_policy["protocol"].as<std::string>();
    }

    policies_.push_back(policy);
    priority++;
  }
  Construct();
  return;
}

std::unique_ptr<std::string> Generator::CreateFromPolicy() {
  std::string t = "\t";
  std::string nl = "\n";
  std::string address_checking;
  std::string action_decition;
  std::string ipaddr_definition;
  bool need_ip_parse = false;

  for (const auto& policy : policies_) {
    int condition_counter = 0;
    int index = policy.priority;

    // Create condition code reflecting policy.
    // |policy_code| represents for one policy.
    std::string policy_code = t + "// priority " + std::to_string(index) + nl;
    std::string condition;

    // protocol
    if (!policy.protocol.empty()) {
      if (policy.protocol == "ICMP" || policy.protocol == "icmp" ||
          policy.protocol == "Icmp") {
        need_ip_parse = true;
        condition += condition_counter ? "&& (iph->protocol == IPPROTO_ICMP) "
                                       : "(iph->protocol == IPPROTO_ICMP) ";
        condition_counter++;
      } else if (policy.protocol == "TCP" || policy.protocol == "tcp") {
        need_ip_parse = true;
        condition += condition_counter ? "&& (iph->protocol == IPPROTO_TCP) "
                                       : "(iph->protocol == IPPROTO_TCP) ";
        condition_counter++;
      } else if (policy.protocol == "UDP" || policy.protocol == "udp") {
        need_ip_parse = true;
        condition += condition_counter ? "&& (iph->protocol == IPPROTO_UDP) "
                                       : "(iph->protocol == IPPROTO_UDP) ";
        condition_counter++;
      }
    }

    // ip address conversion.
    if (!policy.ip_address.empty()) {
      need_ip_parse = true;
      std::string inline__filter_addr = "filter_addr_" + std::to_string(index);
      condition += condition_counter
                       ? "&& (iph->saddr == " + inline__filter_addr + ")"
                       : "(iph->saddr == " + inline__filter_addr + ")";
      std::string ipaddr_string = policy.ip_address;
      ipaddr_definition += t + "__u32 " + inline__filter_addr + " = " +
                           ConvertIPAddressToHexString(ipaddr_string) + ";" +
                           nl;
      condition_counter++;
    }

    if (condition_counter == 1) {
      policy_code += t + "if " + condition + "{" + nl;
    } else {
      policy_code += t + "if (" + condition + ") {" + nl;
    }

    // Create action code.
    switch (policy.action) {
      case Action::Pass:
        policy_code += t + t + "goto out;" + nl;
        break;
      case Action::Drop:
        policy_code +=
            t + t + "action = XDP_DROP;" + nl + t + t + "goto out;" + nl;
    }
    policy_code += t + "}" + nl;

    // |action_decition| is a set of |policy_code|.
    action_decition += policy_code + nl;

  }  // for (const auto& policy : policies_)

  // Create verify code.
  if (need_ip_parse) {
    address_checking += xdp::verify_ip + nl;
  }

  std::unique_ptr<std::string> code = std::make_unique<std::string>(
      address_checking + ipaddr_definition + nl + action_decition);
  return code;
}

void Generator::Construct() {
  std::string nl = "\n";

  // include part.
  std::string include = xdp::include + nl + xdp::include_ip;

  // define part.
  std::string define = xdp::constant + nl + xdp::struct_datarec + nl +
                       xdp::struct_map + nl + xdp::struct_hdr_cursor;

  // inline function.
  std::string inline_func = xdp::inline_func_stats;

  // xdp section.
  std::string policy = *CreateFromPolicy().get();
  std::string func = xdp::func_name + xdp::func_fix + policy + xdp::func_out +
                     xdp::r_bracket + xdp::nl;
  std::string sec = xdp::sec_name + func;

  // license.
  std::string license = xdp::license;

  xdp_prog_ =
      include + nl + define + nl + inline_func + nl + sec + nl + license;
  Write();
  return;
}

void Generator::Write() {
  std::ofstream xdp_file("xdp-generated.c");
  if (!xdp_file) {
    std::cerr << "Cannot open xdp-generated.c" << std::endl;
    exit(EXIT_FAIL);
  }
  xdp_file << xdp_prog_ << std::endl;
  std::cout << "Writing to xdp-generated.c done! " << std::endl;
  return;
}