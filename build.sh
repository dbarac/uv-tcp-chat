#!/usr/bin/bash

gcc chat_server.c -luv -o chat_server
gcc chat_client.c -luv -o chat_client
