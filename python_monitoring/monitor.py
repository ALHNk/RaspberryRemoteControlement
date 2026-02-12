#!/usr/bin/env python3
"""
Real-time Motor Speed Visualizer
Connects to the motor server's broadcast port and plots motor speeds in real-time.
"""

import socket
import json
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque
from datetime import datetime
import argparse
import sys

class MotorSpeedMonitor:
    def __init__(self, host='10.35.97.217', port=5051, max_points=100):
        self.host = host
        self.port = port
        self.max_points = max_points
        self.buffer = ""
        
        # Data storage
        self.timestamps = deque(maxlen=max_points)
        self.motor0_speeds = deque(maxlen=max_points)
        self.motor1_speeds = deque(maxlen=max_points)
        self.motor2_speeds = deque(maxlen=max_points)
        self.motor3_speeds = deque(maxlen=max_points)
        
        # Socket connection
        self.sock = None
        self.connected = False
        
        # Setup plot
        self.fig, self.axes = plt.subplots(2, 2, figsize=(12, 8))
        self.fig.suptitle('Real-time Motor Speed Monitor', fontsize=16)
        
        self.lines = []
        motor_names = ['Motor 0', 'Motor 1', 'Motor 2', 'Motor 3']
        colors = ['blue', 'red', 'green', 'orange']
        
        for idx, (ax, name, color) in enumerate(zip(self.axes.flat, motor_names, colors)):
            ax.set_title(name)
            ax.set_xlabel('Time (s)')
            ax.set_ylabel('Speed')
            ax.grid(True, alpha=0.3)
            line, = ax.plot([], [], color=color, linewidth=2)
            self.lines.append(line)
            ax.set_xlim(0, self.max_points / 10)  # Assuming 10 Hz update rate
            ax.set_ylim(-10, 10)  # Initial range, will auto-adjust
        
        plt.tight_layout()
        
    def connect(self):
        """Connect to the motor server broadcast port"""
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.connect((self.host, self.port))
            self.sock.settimeout(1.0)  # 1 second timeout
            self.connected = True
            print(f"Connected to motor server at {self.host}:{self.port}")
            return True
        except Exception as e:
            print(f"Failed to connect: {e}")
            self.connected = False
            return False
    
    def disconnect(self):
        """Disconnect from server"""
        if self.sock:
            self.sock.close()
        self.connected = False
        print("Disconnected from server")
    
    def read_data(self):
        if not self.connected:
            return None

        try:
            chunk = self.sock.recv(4096).decode()

            if not chunk:
                self.connected = False
                return None

            self.buffer += chunk

            if "\n" not in self.buffer:
                return None

            line, self.buffer = self.buffer.split("\n", 1)

            if not line.strip():
                return None

            return json.loads(line)

        except socket.timeout:
            return None
        except Exception as e:
            print("Read error:", e)
            self.connected = False
            return None


    
    def update_plot(self, frame):
        """Animation update function"""
        if not self.connected:
            # Try to reconnect
            print("Attempting to reconnect...")
            if not self.connect():
                return self.lines
        
        # Read new data
        data = self.read_data()
        if data is None:
            return self.lines
        
        # Extract motor speeds
        try:
            timestamp = len(self.timestamps) * 0.1  # 10 Hz = 0.1s per sample
            self.timestamps.append(timestamp)
            self.motor2_speeds.append(data['motor2'])
            self.motor3_speeds.append(data['motor3'])
            self.motor0_speeds.append(0)
            self.motor1_speeds.append(0)
            
            # Update all plots
            speeds = [self.motor0_speeds, self.motor1_speeds, 
                     self.motor2_speeds, self.motor3_speeds]
            
            for idx, (ax, line, speed_data) in enumerate(zip(self.axes.flat, self.lines, speeds)):
                if len(self.timestamps) > 0:
                    line.set_data(list(self.timestamps), list(speed_data))
                    
                    # Auto-adjust x and y limits
                    ax.set_xlim(max(0, timestamp - self.max_points * 0.1), timestamp + 1)
                    
                    if len(speed_data) > 0:
                        min_speed = min(speed_data)
                        max_speed = max(speed_data)
                        margin = max(1.0, (max_speed - min_speed) * 0.1)
                        ax.set_ylim(min_speed - margin, max_speed + margin)
                        
                        # Update title with current value
                        ax.set_title(f'Motor {idx} (Current: {speed_data[-1]:.3f})')
            
        except KeyError as e:
            print(f"Missing key in data: {e}")
        
        return self.lines
    
    def run(self):
        """Start the visualization"""
        if not self.connect():
            print("Failed to connect. Exiting...")
            sys.exit(1)
        
        # Create animation
        ani = animation.FuncAnimation(
            self.fig, 
            self.update_plot, 
            interval=100,  # 100ms = 10 Hz
            blit=False,
            cache_frame_data=False
        )
        
        try:
            plt.show()
        except KeyboardInterrupt:
            print("\nInterrupted by user")
        finally:
            self.disconnect()


def main():
    parser = argparse.ArgumentParser(description='Monitor motor speeds in real-time')
    parser.add_argument('--host', default='localhost', help='Server hostname or IP (default: localhost)')
    parser.add_argument('--port', type=int, default=5051, help='Broadcast port (default: 5051)')
    parser.add_argument('--max-points', type=int, default=100, help='Maximum points to display (default: 100)')
    
    args = parser.parse_args()
    
    monitor = MotorSpeedMonitor(
        host=args.host,
        port=args.port,
        max_points=args.max_points
    )
    
    monitor.run()


if __name__ == '__main__':
    main()
