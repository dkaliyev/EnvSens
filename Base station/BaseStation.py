#!/usr/bin/python

import socket  # Networking support
import time    # Current time
import json
import serial
import ast
import multiprocessing
import httplib
import datetime
import sys
import os
from ctypes import *


class Server:
    def __init__(self, host, port, serialObj=None):
        """ Constructor """
        self.host = host
        self.port = port

    def activate_server(self):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:   # user provided in the __init__() port may be unavaivable
            print("Launching HTTP server on ", self.host, ":", self.port)
            self.socket.bind((self.host, self.port))

        except Exception as e:
            print("Warning: Could not aquite port:"+self.port+"\n")
            print("I will try a higher port")
            user_port = self.port
            self.port = 50007

            try:
                print("Launching HTTP server on ", self.host, ":", self.port)
                self.socket.bind((self.host, self.port))

            except Exception as e:
                print("ERROR: Failed to acquire sockets for ports ", user_port, " and 8080. ")
                print("Try running the Server in a privileged user mode.")
                self.shutdown()
                import sys
                sys.exit(1)

        print("Server successfully acquired the socket with port:", self.port)
        print("Press Ctrl+C to shut down the server and exit.")
        self._wait_for_connections()

    def shutdown(self):
        """ Shut down the server """
        try:
            print("Shutting down the server")
            s.socket.shutdown(socket.SHUT_RDWR)

        except Exception as e:
            print("Warning: could not shut down the socket. Maybe it was already closed?", e)

    def _gen_headers(self,  code):
        """ Generates HTTP response Headers. Ommits the first line! """

        # determine response code
        h = ''
        if (code == 200):
            h = 'HTTP/1.1 200 OK\n'
        elif(code == 404):
            h = 'HTTP/1.1 404 Not Found\n'

        # write further headers
        current_date = time.strftime("%a, %d %b %Y %H:%M:%S", time.localtime())
        h += 'Date: ' + current_date + '\n'
        h += 'Server: Simple-Python-HTTP-Server\n'
        h += 'Connection: close\n\n'  # signal that the conection wil be closed after complting the request

        return h

    def _wait_for_connections(self):
        """ Main loop awaiting connections """
        while True:
            command_parser = CommandParser()
            print("Awaiting New connection")
            self.socket.listen(3)  # maximum number of queued connections

            conn, addr = self.socket.accept()
            # conn - socket to client
            # addr - clients address

            print("Got connection from:", addr)

            data = conn.recv(1024)  # receive data from client
            string = bytes.decode(data)  # decode it to string

            #get header
            header = command_parser.get_header(string)

            #determine request method  (HEAD and GET are supported)
            request_method = command_parser.get_method(header)
            content = command_parser.get_content(header)
            print("Method: " + request_method)
            print("Request body: " + string)
            print("Header: " + header)
            print("Content: " + content)

            if (request_method == 'GET') | (request_method == 'POST'):
                #get requested URL
                url_requested = command_parser.get_url(header)
                #get arguments string from URL    
                args_str = command_parser.get_args(url_requested)
                #parse each argument and make a list
                arguments_requested = command_parser.parse_arguments(args_str)

                print("Url requested: " + url_requested)
                print("Arguments are: ", command_parser.args_dic_to_string(arguments_requested))
                # generate response header
                response_headers = self._gen_headers(200)
                # generate response content
                response_content = "test answer"
                # return headers for GET and HEAD
                server_response = response_headers.encode()
                # return additional content for GET only
                if (request_method == 'GET'):
                    server_response += response_content

                conn.send(server_response)
                print("Closing connection with client")
                conn.close()

            else:
                print("Unknown HTTP request method:", request_method)

    def graceful_shutdown(sig, dummy):
        """ This function shuts down the server. It's triggered
        by SIGINT signal """
        s.shutdown()  # shut down the server
        import sys
        sys.exit(1)


class CommandParser():
    def __init__(self):
        self.dict_args = []
        self.args = ""
        self.header = ""
        self.content = ""
        self.method = ""
        self.url = ""
        self.action = ""

    def get_method(self, header):
        self.method = header.split(' ')[0]
        return self.method

    def get_action(self, url):
        action = url.split('/')
        if(len(action) == 3 & action[0] == ''):
            self.action = action[2]
        else:
            print("Invalid url format")
        return self.action

    def get_url(self, header):
        self.url = header.split(' ')[1]
        return self.url

    def get_header(self, data):
        self.header = data.split('\r\n\r\n')[0]
        return self.header

    def get_content(self, data):
        tmp = data.split('\r\n\r\n')
        if(len(tmp) > 1):
            self.content = tmp[0]
        return self.content

    def get_args(self, url):
        args = url.split('?')
        if(len(args) > 1):
            self.args = args[1]
        return self.args

    def parse_arguments(self, args_p):
        args = args_p
        self.dict_args = []
        parsed_args = args.split('&')
        for arg in parsed_args:
            arg_list = arg.split('=')
            if(len(arg_list) > 1):
                self.dict_args.append({arg_list[0]: arg_list[1]})
        return self.dict_args

    def args_dic_to_string(self, args_dic):
        return json.dumps([dict(pn) for pn in args_dic])


"""class timespec(ctypes.Structure):
    _fields_ = [("tv_sec", ctypes.c_long),
                ("tv_nsec", ctypes.c_long)]"""


class MainController():
    def __init__(self):
        self.serialObj = serial.Serial(
            port="/dev/ttyAMA0",
            baudrate=115200,
	    timeout=None,
	    bytesize=8,
	    parity='N',
	    stopbits=1)
        self.CLOCK_REALTIME = 0

    def server(self, serialObj):
        print("Starting web server")
        s = Server('192.168.1.3', 50007, serialObj)  # construct server object
        s.activate_server()  # aquire the socket

    def uart_reader(self, serialObj):
        print("Wating for readings")
        while 1:
            read = serialObj.readline()
            connection = httplib.HTTPConnection('192.168.1.2', 80)
            while connection=={}:
                print "Trying to connect...again"
                connection = httplib.HTTPConnection('192.168.1.2', 80)
            str = ast.literal_eval(read)
            avg = int(str['Raw'])
            numCycles = str['numCycles']
            date_string = str['date']
            sens_id = str['SensId']
            value = int(avg/numCycles)
            headers = {"Content-type": "application/json"}
            data = {'Reading': value, 'date': date_string, 'SensorId': sens_id}
            print str
            print data
            connection.request("POST", "/api/readings", json.dumps(data), headers)
            response = connection.getresponse()
            #print response.status, response.reason
            data = response.read()
        print "Done. Closing connection"
        connection.close()

    def get_time_from_server(self):
        headers = {"Accept": "application/json"}
        print("Connecting to server...")
        connection = httplib.HTTPConnection('192.168.1.2', 80)
        while connection == {}:
            print("Trying to connect to server...")
            connection = httplib.HTTPConnection('192.168.1.2', 80)
        try:
            connection.request("GET", "/api/time", "", headers)
            response = connection.getresponse()
            time_val = response.read()
            connection.close()
            return time_val
        except:
            print("Server doesnt respond")
            return ""

    def set_time(self, time_mls):
        self.time_obj = datetime.datetime.utcfromtimestamp(time_mls//1000)
        os.system('sudo date -s "'+self.time_obj.isoformat()+"\"")

    def send_time(self, serialObj):
        time_in_bytes = bytearray(datetime.datetime.now().isoformat()+'\0', "UTF-8")
        time_str = datetime.datetime.now().isoformat()
        p = create_string_buffer(time_str)
        print("Sending time "+p.value)
        print("Raw "+p.raw)
        #for b in bytearray(time_str, "UTF-8"):
        serialObj.write(p.raw)
        print("Time sent. Expecting answer")
        answer = serialObj.readline()
        print(answer)



    def run(self):
        time_mls = self.get_time_from_server()
        print("response from server "+time_mls)
        while time_mls == "":
            time_mls = self.get_time_from_server()
            print("response from server "+time_mls)
        print("Setting time")
        try:
            self.time_mls = long(float(time_mls))
        except ValueError, e:
            print("Error "+e)
        self.set_time(self.time_mls)
        self.send_time(self.serialObj)
        serverProc = multiprocessing.Process(target=self.server, args=(self.serialObj,)) # Server process
        uartProc = multiprocessing.Process(target=self.uart_reader, args=(self.serialObj,)) # UART process
        serverProc.start()
        uartProc.start()
        serverProc.join()
        uartProc.join()


if __name__ == '__main__':
    mainController = MainController()
    print("Time before")
    print datetime.datetime.now()
    mainController.run()
    print datetime.datetime.now()
