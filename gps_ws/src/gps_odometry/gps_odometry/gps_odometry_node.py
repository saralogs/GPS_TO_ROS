from pyparsing import line
import rclpy
from rclpy.node import Node

from std_msgs.msg import String
from nav_msgs.msg import Odometry
from geometry_msgs.msg import Quaternion, TransformStamped
from tf2_ros import TransformBroadcaster

import math

EARTH_RADIUS = 6378137.0  # meters


class GpsOdometry(Node):
    def __init__(self):
        super().__init__('gps_odometry')

        # Subscriber to your serial node
        self.sub = self.create_subscription(
            String,
            '/serial_data',
            self.serial_callback,
            10
        )

        # Odometry publisher
        self.odom_pub = self.create_publisher(
            Odometry,
            '/gps_odom',
            10
        )

        # TF broadcaster
        self.tf_broadcaster = TransformBroadcaster(self)

        # State
        self.origin_set = False
        self.lat0 = 0.0
        self.lon0 = 0.0

        self.prev_x = None
        self.prev_y = None
        self.prev_time = None

        self.get_logger().info('GPS odometry node started')

    def serial_callback(self, msg: String):
        try:
            lat, lon = self.parse_gps(msg.data)
        except ValueError:
            self.get_logger().warn(f'Invalid GPS data: {msg.data}')
            return

        now = self.get_clock().now()

        # Set origin on first fix
        if not self.origin_set:
            self.lat0 = math.radians(lat)
            self.lon0 = math.radians(lon)
            self.origin_set = True
            self.prev_time = now
            self.get_logger().info('GPS origin set')
            return

        # Convert lat/lon to local Cartesian (ENU)
        lat_rad = math.radians(lat)
        lon_rad = math.radians(lon)

        x = EARTH_RADIUS * (lon_rad - self.lon0) * math.cos(self.lat0)
        y = EARTH_RADIUS * (lat_rad - self.lat0)

        # Time delta
        dt = (now - self.prev_time).nanoseconds * 1e-9
        self.prev_time = now

        if self.prev_x is None or dt <= 0.0:
            vx = 0.0
            vy = 0.0
        else:
            vx = (x - self.prev_x) / dt
            vy = (y - self.prev_y) / dt

        # Heading from velocity
        yaw = math.atan2(vy, vx) if (vx != 0.0 or vy != 0.0) else 0.0

        q = Quaternion()
        q.z = math.sin(yaw / 2.0)
        q.w = math.cos(yaw / 2.0)

        # Publish odometry
        odom = Odometry()
        odom.header.stamp = now.to_msg()
        odom.header.frame_id = 'odom'
        odom.child_frame_id = 'base_link'

        odom.pose.pose.position.x = x
        odom.pose.pose.position.y = y
        odom.pose.pose.orientation = q

        odom.twist.twist.linear.x = vx
        odom.twist.twist.linear.y = vy

        self.odom_pub.publish(odom)

        # Publish TF
        self.publish_tf(now, x, y, q)

        self.prev_x = x
        self.prev_y = y

    def parse_gps(self, line: str):
        """
        Expected format:
        LAT:11.321456,LON:75.934567
        """
        try:
            parts = line.split(',')

            lat_str = parts[0].split(':')[1]
            lon_str = parts[1].split(':')[1]

            lat = float(lat_str)
            lon = float(lon_str)

            return lat, lon
        except (IndexError, ValueError):
            raise ValueError(f"Invalid GPS format: {line}")

    def publish_tf(self, stamp, x, y, q):
        t = TransformStamped()
        t.header.stamp = stamp.to_msg()
        t.header.frame_id = 'odom'
        t.child_frame_id = 'base_link'

        t.transform.translation.x = x
        t.transform.translation.y = y
        t.transform.translation.z = 0.0
        t.transform.rotation = q

        self.tf_broadcaster.sendTransform(t)


def main():
    rclpy.init()
    node = GpsOdometry()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()
