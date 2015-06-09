#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
messaging.py

A simple messaging protocol
"""
__author__  = "Fin"

# Stdlib Imports
import SocketServer
import socket
import sys
from time import ctime

# Third Party Imports

# finpack Imports
import finpack


@finpack.Compile
class ChatMessage(finpack.Message):
    name = finpack.STRING_TYPE(0, 18)
    ip = finpack.STRING_TYPE(0, 15)
    message = finpack.STRING_TYPE(0, 32)


class MyTCPHandler(SocketServer.BaseRequestHandler):

    def handle(self):
        raw = self.request.recv(1024).rstrip()

        # Unpack the message with ChatMessage.unpack
        name, ip, message = ChatMessage.unpack(raw)
        print "[{}] {}@{} wrote: {}".format(ctime(), name.rstrip("\x00"), ip.rstrip("\x00"), message.rstrip("\x00"))
        self.request.sendall("OK")


def start_server(host, port):
    server = SocketServer.TCPServer((host, port), MyTCPHandler)
    server.serve_forever()


def start_client(host, port):
    ip = socket.gethostbyname(socket.gethostname())
    name = sys.argv[4]
    message = " ".join(sys.argv[5:])

    # Pack the message using ChatMessage.pack using positional
    # arguments in the order declared on the class
    packed = ChatMessage.pack(name, ip, message)
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.connect((host, port))
        sock.sendall(packed)
        sock.recv(1024)
    finally:
        sock.close()


def main():
    usage = "usage: {} [server|client] host port [name, message]".format(sys.argv[0])
    if len(sys.argv) < 4:
        print usage
        exit()
    mode = sys.argv[1]
    host = sys.argv[2]
    port = int(sys.argv[3])
    if mode == "client":
        start_client(host, port)
    else:
        start_server(host, port)

if __name__ == '__main__':
    main()
