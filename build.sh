#!/usr/bin/bash

# Multi-user TCP chat server
gcc chat_server.c -luv -o chat_server
gcc chat_client.c -luv -o chat_client

# UDP example (send and receive)
gcc udp_send.c -luv -o udp_send
gcc udp_receive.c -luv -o udp_receive

# Polling example
gcc poll_example.c -luv -o poll_example

# Packet dissector
gcc packet_dissector.c -luv -o packet_dissector
