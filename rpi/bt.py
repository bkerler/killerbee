import bluetooth, os, time, sys, threading

# The in directory for new pcap files
PCAP_DIR = "/tmp/pcaps"
FIFOPATH = '/tmp/gpsfifo'
SERVICE_NAME = "EyeOfTechnology"
LOGFILE = "/var/log/iot.log"
is_running = True


def _curr_time():
    return time.strftime("%Y-%m-%d %H:%M:%S")


def _format_log(logstring):
    return _curr_time() + ": " + logstring + "\n"

"""
    def bt_loop(ld):
        '''
        Connects to a device and then transmits pcaps.
        '''
        ld.write(_format_log("Staring service"))
        sock=bluetooth.BluetoothSocket(bluetooth.RFCOMM)
        ld.write(_format_log("Got bluetooth socket"))

        # All the services with this name should be fine
        service_desc = get_connection(ld)

        # Getting service information
        port = service_desc['port']
        target_address = service_desc['host']

        # Connecting to the device
        sock.connect((target_address, port))
        ld.write(_format_log("Connected to android device"))
        while True:
            # Loop through the in directory and send over files
            time.sleep(2)
            files = os.listdir(PCAP_DIR)
            for f in files:
                fd = open(PCAP_DIR + '/' + f, 'rb')
                temp = fd.read()
                sock.send(temp)
                ld.write(_format_log("Sending " + f))
                fd.close()
                os.remove(PCAP_DIR + "/" + f)
            """


"""
    def receive_loop(ld):
        ld.write(_format_log("Staring service"))
        sock=bluetooth.BluetoothSocket(bluetooth.RFCOMM)
        ld.write(_format_log("Got bluetooth socket"))

        # All the services with this name should be fine
        service_desc = get_connection(ld)

        # Getting service information
        port = service_desc['port']
        target_address = service_desc['host']

        # Connecting to the device
        sock.connect((target_address, port))
        ld.write(_format_log("Connected to android device"))
        while True:
            time.sleep(2)
            print "Getting data"
            data = sock.recv(1024)
            print "Data: " + data
        """


def send_data(ld, sock):
    global is_running
    while is_running:
        # Loop through the in directory and over files
        time.sleep(2)
        files = os.listdir(PCAP_DIR)
        for f in files:
            fd = open(PCAP_DIR + '/' + f, 'rb')
            temp = fd.read()
            sock.send(temp)
            ld.write(_format_log("Sending " + f))
            fd.close()
            os.remove(PCAP_DIR + "/" + f)
    print "stopped from send"


def receive_data(ld, sock):
    global is_running
    while is_running:
        time.sleep(7)
        print "Getting data"
        data = sock.recv(1600)
        with open (FIFOPATH, 'a') as fd:
            fd.write(data + "\n")
        print "Data: " + data
    print "stopped from receive"


def connect_bluetooth(ld):
    socket = get_bluetooth_socket(ld)
    # any service with the name should be fine
    service = get_connection(ld, SERVICE_NAME)[0]
    socket.connect((service['host'], service['port']))

    ld.write(_format_log("Connected to android device"))
    return socket


def get_bluetooth_services(ld, name):
    services = []
    while len(services) < 1:
        try:
            # Search for the service
            services = bluetooth.find_service(name=name)
        except bluetooth.btcommon.BluetoothError as e:
            error_msg = str(e)
            if not error_msg == "error accessing bluetooth device":
                ld.write(_format_log(str(e)))
    return services


def get_bluetooth_socket(ld):
    sock = bluetooth.BluetoothSocket(bluetooth.RFCOMM)
    ld.write(_format_log("Got bluetooth socket"))
    return sock


def setup_fifo(path):
    if not os.path.exists(path):
        os.mkfifo(path)


def setup_logs(path):
    if os.path.isfile(path):
        return open(path, 'a', 0)
    else:
        return open(path, 'w', 0)


def start_threads(ld, sock):
    s = threading.Thread(target=send_data, args=(ld, sock))
    r = threading.Thread(target=receive_data, args=(ld, sock))
    s.start()
    r.start()
    return s, r


def handle_exception(ld, e, sock):
    is_running = False
    sock.close()

    ld.write(_format_log(str(e)))
    print "Out of both threads"

    is_running = True
    ld.write(_format_log("Restarting service"))


if __name__=="__main__":
    ld.write(_format_log("Starting service"))
    setup_fifo(FIFOPATH)
    ld = setup_logs(LOGFILE)

    while True:
        try:
            socket = connect_bluetooth(ld)

            s, r = start_threads(ld, socket)
            s.join()
            r.join()
        except Exception as e:
            handle_exception(ld, e, socket)
