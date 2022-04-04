#!/usr/bin/python3

import socket, struct
import sys, getopt, re
import urllib3, time


class TFTPParamaters:
   """Class containing the used TFTP Parameters."""
   MODE = b"octet"
   PORT = 69
   DATASIZE = 512


class TFTPOpcodes:
    """Class containing all the opcodes used in TFTP."""
    RRQ = b'\x00\x01'
    WRQ = b'\x00\x02'
    DATA = b'\x00\x03'
    ACK = b'\x00\x04'
    ERROR = b'\x00\x05'
    OACK = b'\x00\x06'


class TFTPErrorCodes:
    """Class containing all the error codes and their messages used in TFTP."""
    UNKNOWN = 0
    FILE_NOT_FOUND = 1
    ACCESS_VIOLATION = 2
    DISK_FULL = 3
    ILLEGAL_OPERATION = 4
    UNKNOWN_TRANSFER_ID = 5
    FILE_EXISTS = 6
    NO_SUCH_USER = 7
    INVALID_OPTIONS = 8

def tftp_send(IP, content, timeout, mode):

   sock = socket.socket(socket.AF_INET6, # Internet
                        socket.SOCK_DGRAM) # UDP
   sock.settimeout(timeout)

   DATA = TFTPOpcodes.WRQ
   DATA += b"firmware.bin"   #trivial for dualmote's TFTP ...
   DATA += b'\x00'
   DATA += TFTPParamaters.MODE
   DATA += b'\x00'

   sock.sendto(DATA, (IP, TFTPParamaters.PORT))
   print("UDP target IP:", IP, "port", TFTPParamaters.PORT, "message:", DATA)

   #TFTP sets up a different socket using the Source port set by client and Destination port set by server (instead of TFTP's default port 69)

   hostIP, hostPort, x, y = sock.getsockname() #Save the used IP address and randomly generated source port
   sock.close() #Close initial socket made to server on TFTP's default port 69, we need to rebind the same source port!


   print("Opening new data socket on IP %s port %d" % (hostIP, hostPort))
   #Set up new data socket which listens on the source port for a response from the server on a random port (not TFTP's default port 69)
   datasock = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
   datasock.settimeout(timeout)
   datasock.bind((hostIP, hostPort))

   try:
      data, addr = datasock.recvfrom(32)  # Just some arbitrary value for now. TFTP (N)ACK is quite short
      #print("received message: %s from %s port %d" % (data, addr[0], addr[1]))
      datasock.connect(addr)
   except socket.timeout:
      print("Timeout receiving TFTP response for node on IP %s port %d" % (IP, TFTPParamaters.PORT))
      if mode == 'tftp':
         sys.exit(2)
      return False


   try:
      currentBlock = 1
      while (currentBlock-1)*512 < len(content):

         data = TFTPOpcodes.DATA
         data += struct.pack('!H', currentBlock) #uint16_t
         if((currentBlock)*512 <= len(content)):
            data += content[(currentBlock-1)*512:currentBlock*512]
         else:
            #print("Last block")
            data += content[(currentBlock-1)*512:]
            #print("Last byte:", '{:02x}'.format(data[-1]), "Type:", type(data[-1]))
            if(data[-1] != 0):      #do we need this? Wireshark showed differences with curl
               print("Adding additinal 0x00 byte to indicate last TFTP tx.")
               data += b'\x00'

         for attempt in range(1,6):
            try:
               datasock.send(data)
               data = datasock.recv(520)  # buffer size is 512B + some overhead
               #print("received message: %s" % (data))
               block = struct.unpack('!H', data[2:4])
               if(data[0:2] == TFTPOpcodes.ACK):
                  print("Received ACK for block %d" % block)
                  currentBlock += 1
                  break
               else:
                  print("Received status %d for block %d. Attempt %d" % (status, block, attempt))

            except socket.timeout:
               print("Timeout on block", currentBlock, "attempt", attempt)
         else:
            print("Exceeded attempts. Closing socket and terminating.")
            datasock.close
            if mode == 'tftp':
               sys.exit(2)
            return False                    
      
      print("Done sending content. Closing socket.")
      datasock.close()

   except KeyboardInterrupt:
      raise

   return True

def main(argv):
   IP = ""
   filename = ""
   timeout = 10

   try:
        opts, args = getopt.getopt(argv[1:],"hT:t:",["file=", "timeout="])
   except getopt.GetoptError:
        print(argv[0] + ' -T <file> tftp://[IP]')
        sys.exit(2)
   for opt, arg in opts:
      if opt == '-h':
         print(argv[0] + ' -T <file> -t <timeout> tftp://[IP]')
         sys.exit()
      elif opt in ("-T", "--file"):
         filename = arg
      elif opt in ("-t", "--timeout"):
         try:
            if int(arg) > 0: 
               timeout = int(arg)
            else:
               print(argv[0] + ' -T <file> -t <timeout> tftp://[IP]')
               print("The value of timeout must be greater than 0.")

         except ValueError:   
            print(argv[0] + ' -T <file> -t <timeout> tftp://[IP]')
            print("The value of timeout must be an integer.")
            

   for arg in args:
      regex = re.search(r"^tftp:\/\/\[?(.*?)\]?\/?$", arg)
      if(regex and IP == ""):
         IP = regex.group(1)
         mode = 'tftp'

   for arg in args:
      regex = re.search(r"^http:\/\/\[?(.*?)\]?\/?$", arg)
      if(regex and IP == ""):
         IP = regex.string
         mode = 'http'
   
   if IP == "" or filename == "":
      print(argv[0] + ' -T <file> -t <timeout> tftp://[IP]')
      sys.exit(2)

   try:
      f = open(filename, 'rb')
      content = f.read()
      f.close()
   except OSError:
      print("Cannot open", filename)
      sys.exit(2)   
   
   if mode == 'tftp':
      print(IP, filename, timeout)
      tftp_send(IP, content, timeout, mode) 
   if mode == 'http':
      print(IP, filename, timeout)
      http = urllib3.PoolManager()
      try:
         r = http.request('GET', IP)
         htmlstr = r.data.decode('utf-8')
      except Exception as e:
         print("Could not get html page from BR at ", IP)
         sys.exit(2)

      htmlstr = re.sub('/[0-9]*', '', htmlstr)        # depending on RPL conf, addresses might end in /128 etc.
      address_list= re.findall("<li>(.*) \(", htmlstr)
      print(address_list)
      failed_addresses = []
      perf_times = []
      for address in address_list:
         print(address, filename, timeout)
         start = time.perf_counter()
         sending_success = tftp_send(address, content, timeout, mode)
         perf_times.append(time.perf_counter() - start)
         if not sending_success:
            failed_addresses.append(address)
         
      if failed_addresses!=[]:
         print("Failed to send to", failed_addresses)
      else:
         print("Successfully sent to every mote in RPL tree")

      print("Sent to", len(address_list)-len(failed_addresses), "out of", len(address_list),\
            "motes, taking ", perf_times)


if __name__ == "__main__":
   main(sys.argv)   
