import rclpy
from rclpy.node import Node
from std_msgs.msg import String
import serial


class SerialReader(Node):
    def __init__(self):
        super().__init__('serial_reader')

        # Declare parameters
        self.declare_parameter('port', '/dev/ttyUSB0')
        self.declare_parameter('baudrate', 115200)

        port = self.get_parameter('port').get_parameter_value().string_value
        baudrate = self.get_parameter('baudrate').get_parameter_value().integer_value

        # Publisher
        self.publisher_ = self.create_publisher(String, 'serial_data', 10)

        # Open serial port
        try:
            self.ser = serial.Serial(port, baudrate, timeout=1)
            self.get_logger().info(f'Opened serial port {port} at {baudrate}')
        except serial.SerialException as e:
            self.get_logger().error(str(e))
            raise e

        # Timer to read serial
        self.timer = self.create_timer(0.05, self.read_serial)

    def read_serial(self):
        if self.ser.in_waiting > 0:
            try:
                line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    msg = String()
                    msg.data = line
                    self.publisher_.publish(msg)
                    self.get_logger().debug(f'Published: {line}')
            except Exception as e:
                self.get_logger().warn(f'Serial read error: {e}')


def main(args=None):
    rclpy.init(args=args)
    node = SerialReader()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()
