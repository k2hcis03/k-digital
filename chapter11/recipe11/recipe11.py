import time  
import fcntl    
import time  
import os
import struct
import ctypes

IOCTL_MAGIC = ord('R')

class StructFMT(ctypes.Structure):  
    _pack_ = 1  
    _fields_ = [('data',ctypes.c_int)]  


class IOCTLRequest():
    IOCPARM_MASK = 0x3fff
    IOC_NONE = 0x20000000
    IOC_WRITE = 0x40000000
    IOC_READ = 0x80000000

    def FIX(self,x): 
        return struct.unpack("i", struct.pack("I", x))[0]
    def _IO(self,type,number): 
        return self.FIX(self.IOC_NONE|type<<8|number)
    def _IOR(self,type,number,size): 
        return self.FIX(self.IOC_READ|((size&self.IOCPARM_MASK)<<16)|type<<8|number)
    def _IOW(self,type,number,size): 
        return self.FIX(self.IOC_WRITE|((size&self.IOCPARM_MASK)<<16)|type<<8|number)
    def _IOWR(self,type,number,size): 
        return self.FIX(self.IOC_READ|self.IOC_WRITE|((size&self.IOCPARM_MASK)<<16)|type<<8|number)

def main():  
    try: 
        _ioctl = IOCTLRequest()
        dev = os.open("/dev/recipedev", os.O_RDWR)

        data = StructFMT(1)
        SET_DATA = _ioctl._IOW(IOCTL_MAGIC, 2, ctypes.sizeof(data))
        fcntl.ioctl(dev,SET_DATA,data)

        time.sleep(2)
        data = StructFMT(0)
        SET_DATA = _ioctl._IOW(IOCTL_MAGIC, 2, ctypes.sizeof(data))
        fcntl.ioctl(dev,SET_DATA,data)

        os.close(dev)  
    except OSError as e:  
        print(e.errno, "Fail to open device file: /dev/recipedev.")  
        print(e.strerror)  
        os.close(dev)  
    
if __name__ == "__main__":  
    main()  
