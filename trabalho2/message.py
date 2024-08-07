from shared import *

class Message:
    def __init__(self, 
                 source, 
                 dest, 
                 type, 
                 data,
                 read):
        self.source = source
        self.dest = dest
        self.type = type
        self.data = data
        self.read = read