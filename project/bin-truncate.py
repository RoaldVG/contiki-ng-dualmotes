#!/bin/python3

import re, time
import sys, getopt


def crop(infile, outfile):

    fin = open(infile, 'rb')
    content = fin.read()
    fin.close()

    print("Read from "+str(infile)+". Original size is " + str(len(content)), " bytes.")
    #print(''.join('{:02x} '.format(x) for x in content[:21000]))
    startcrop = len(content)

    starttime = time.time()
    searchMatch = re.search(b'\xff{512}', content)
    stoptime = time.time()
    print("Searching at least a repetition of 512B of 0xFF took", stoptime-starttime, "seconds.")

    if(searchMatch):
        startcrop = searchMatch.start() #look for at least 1024 bytes of FF!
        print("Found a repetition. Will truncate from address", hex(startcrop))

        bytesToAdd = (4-(startcrop%4)) if startcrop%4!=0 else 0
        if(bytesToAdd): 
            print("Adding", bytesToAdd, "bytes to make the output a multiple of 4.")
            startcrop += bytesToAdd

    else:
        print("Did not find a large repetition. Outputting the original file")

    print("Output file size:", startcrop, "bytes")
    fout = open(outfile, 'wb')
    fout.write(content[:startcrop])
    fout.close()
    print("Wrote output binary to", outfile)    


def main(argv):
    infile = ""
    outfile = ""

    try:
        opts, args = getopt.getopt(argv[1:],"hi:o:",["in=","out="])
    except getopt.GetoptError:
        print(argv[0] + '-i <inputfile> -o <outputfile>')
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print(argv[0] + '-i <inputfile> -o <outputfile>')
            sys.exit()
        elif opt in ("-i", "--in"):
            infile = arg
        elif opt in ("-o", "--out"):
            outfile = arg

    if infile == "" or outfile == "":
        print(argv[0] + '-i <inputfile> -o <outputfile>')
        sys.exit(2)        

    crop(infile, outfile)
    

if __name__ == "__main__":
   main(sys.argv)