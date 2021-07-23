# import fcntl
import os

def main():
    try:
        dev = os.open("/dev/recipedev", os.O_RDWR)
        data = bytes([ord('A'),ord('B'),ord('C'),ord('D')])
        os.write(dev,data)
        cnt = os.read(dev, 8)
        print(cnt)
        os.close(dev)
    except OSError as e:
        print(e.errno, "Fail to open device file: /dev/recipedev.")
        print(e.strerror)

if __name__ == "__main__":
    main()