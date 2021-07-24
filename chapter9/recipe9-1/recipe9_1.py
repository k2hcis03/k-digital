import os
import select

def main():
    dev = None
    try:
        # dev = os.open("/dev/recipedev", os.O_RDWR)
        # dev = os.open("/sys/devices/platform/foo/foo_notify"\
                        # , os.O_RDONLY)
        dev = os.open("/sys/devices/platform/recipe9_1/recipe_sys/period"\
                         , os.O_RDONLY)
        value = os.read(dev, 100)
        print(value)
        recipePoll = select.epoll()
        recipePoll.register(dev, select.EPOLLPRI)
        index = 0
        while True:
            events = recipePoll.poll()
            print("EPoll result: %s" % (str(events),))

            for fd, event_type in events:
                if event_type & select.EPOLLPRI:
                    print("-EPOLLPRI!")
                os.lseek(dev, 0, os.SEEK_SET)
                value = os.read(dev, 100)
                index += 1
                print(value, index)
    except OSError as e:
        print(e.strerror)
    except KeyboardInterrupt as e:
        print(e)
        os.close(dev)

if __name__ == "__main__":
    main()