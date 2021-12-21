#!/usr/bin/env bash
ip_protocol(){
	# raw ip with protocol option set.
	# ICMP
	ip netns exec outside hping3 172.16.10.1 --rawip --ipproto 1 -c 1 -i u10

	# TCP
	ip netns exec outside hping3 172.16.10.1 --rawip --ipproto 6 -c 2 -i u10

	# UDP
	ip netns exec outside hping3 172.16.10.1 --rawip --ipproto 17 -c 3 -i u10
}

ip_saddr(){
	# use fake IP source address.
	ip netns exec outside hping3 172.16.10.1 --spoof 192.0.2.0 -c 1 -i u10

	ip netns exec outside hping3 172.16.10.1 --spoof 192.0.2.254 -c 2 -i u10

	ip netns exec outside hping3 172.16.10.1 --spoof 198.51.100.10 -c 3 -i u10

	ip netns exec outside hping3 172.16.10.1 --spoof 198.51.100.100 -c 4 -i u10

	ip netns exec outside hping3 172.16.10.1 --spoof 203.0.113.1 -c 5 -i u10
}

ip_tos(){
	# Type of Service field is 00000000
	ip netns exec outside hping3 172.16.10.1 --tos 0 -c 1 -i u10

	# 00000010
	ip netns exec outside hping3 172.16.10.1 --tos 2 -c 2 -i u10

	# 00000100
	ip netns exec outside hping3 172.16.10.1 --tos 4 -c 3 -i u10

	# 00001000
	ip netns exec outside hping3 172.16.10.1 --tos 8 -c 4 -i u10

	# 00001010
	ip netns exec outside hping3 172.16.10.1 --tos 10 -c 5 -i u10
}

ip_ttl(){
	# Time To Live is 1.
	ip netns exec outside hping3 172.16.10.1 --ttl 255 -c 1 -i u10

	# TTL 16.
	ip netns exec outside hping3 172.16.10.1 --ttl 64 -c 2 -i u10

	# TTL 32.
	ip netns exec outside hping3 172.16.10.1 --ttl 32 -c 3 -i u10

	# TTL 64.
	ip netns exec outside hping3 172.16.10.1 --ttl 16 -c 4 -i u10

	# TTL 255.
	ip netns exec outside hping3 172.16.10.1 --ttl 1 -c 5 -i u10
}

case $1 in
	"ip_protocol") ip_protocol;;
	"ip_saddr") ip_saddr;;
	"ip_tos") ip_tos;;
	"ip_ttl") ip_ttl;;
	*) echo "invalid argument";;
esac