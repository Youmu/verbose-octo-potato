# Class Serial Interface
Class name: SerialInterface

# Member Functions

- `SerialInterface(const string &device, int boudrate)`   
  Constructor. Opens the UART device with given boudrate.

- `Start()`  
  Start monitoring the serial port.

- `Stop()`  
  Called from another thread to stop waiting for new msgs.

- `SetOnReceive(SerialReceiveCb onReceive)`  
  Set the callback when a line is received from the serial port.

- `SendMessage(const string &msg)`  
  Send a line of message to the serial port, ending with <CR>.

# Data Types
- `typedef void(*SerialReceiveCb)(const string &message)`  
  The callback function when a line of message is received.

# Behavior
When Start is called. It reads messages from the serial port in a new thread.
When a line of message is received. It calls the receive callback set in SetOnReceive.



