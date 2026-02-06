from setuptools import find_packages, setup

package_name = 'serial_reader'

setup(
    name=package_name,
    version='0.0.0',
    packages=[package_name],
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='saras',
    maintainer_email='shameensara@gmail.com',
    description='Serial Reader for ROS2 Jazzy',
    license='Apache License 2.0',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'serial_node = serial_reader.serial_node:main',
        ],
    },
)
