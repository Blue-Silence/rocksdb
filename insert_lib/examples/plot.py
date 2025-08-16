#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Data Visualization Script
Reads data.log file and creates 3D charts
X-axis: Thread count, Memtable size
Y-axis: Operations per unit time (Total op count / Maximum runtime)
"""

import numpy as np
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
import pandas as pd
import argparse
import sys

def read_data_log(filename):
    """Read data.log file"""
    data = []
    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            if line:
                parts = line.split()
                if len(parts) >= 4:  # At least 4 columns: thread count, memtable size, total op count, at least one time
                    thread_count = int(parts[0])
                    memtable_size = int(parts[1])
                    total_op_count = int(parts[2])
                    # Subsequent columns are times, find the maximum
                    times = [int(t) for t in parts[3:]]
                    max_time = max(times)
                    
                    # Calculate operations per millisecond
                    ops_per_ms = total_op_count / max_time if max_time > 0 else 0
                    
                    data.append({
                        'thread_count': thread_count,
                        'memtable_size': memtable_size,
                        'total_op_count': total_op_count,
                        'max_time': max_time,
                        'ops_per_ms': ops_per_ms,
                        'times': times
                    })
    
    return data

def create_3d_plot(data):
    """Create 3D chart"""
    # Extract data
    thread_counts = [d['thread_count'] for d in data]
    memtable_sizes = [d['memtable_size'] for d in data]
    ops_per_ms = [d['ops_per_ms'] for d in data]
    
    # Create grid
    unique_threads = sorted(list(set(thread_counts)))
    unique_memtable_sizes = sorted(list(set(memtable_sizes)))
    
    # Create data matrix
    Z = np.zeros((len(unique_memtable_sizes), len(unique_threads)))
    
    for d in data:
        i = unique_memtable_sizes.index(d['memtable_size'])
        j = unique_threads.index(d['thread_count'])
        Z[i, j] = d['ops_per_ms']
    
    # Convert to k units for display
    X, Y = np.meshgrid(unique_threads, [x/1000 for x in unique_memtable_sizes])
    
    # Create 3D chart
    fig = plt.figure(figsize=(12, 8))
    ax = fig.add_subplot(111, projection='3d')
    
    # Plot 3D surface
    surf = ax.plot_surface(X, Y, Z, cmap='viridis', alpha=0.8)
    
    # Add scatter plot to show actual data points
    ax.scatter(thread_counts, [x/1000 for x in memtable_sizes], ops_per_ms, 
               c='red', marker='o', s=50, label='Data Points')
    
    # Set labels
    ax.set_xlabel('Thread Count')
    ax.set_ylabel('Memtable Size (k)')
    ax.set_zlabel('ops/ms')
    ax.set_title('Multi-threaded Memtable Performance Analysis')
    
    # Add color bar
    fig.colorbar(surf, ax=ax, shrink=0.5, aspect=5)
    
    # Set axis ranges
    ax.set_xlim(min(unique_threads), max(unique_threads))
    ax.set_ylim(min(unique_memtable_sizes)/1000, max(unique_memtable_sizes)/1000)
    
    plt.tight_layout()
    return fig, ax

def create_2d_heatmap(data):
    """Create 2D heatmap"""
    # Extract data
    unique_threads = sorted(list(set([d['thread_count'] for d in data])))
    unique_memtable_sizes = sorted(list(set([d['memtable_size'] for d in data])))
    
    # Create data matrix
    Z = np.zeros((len(unique_memtable_sizes), len(unique_threads)))
    
    for d in data:
        i = unique_memtable_sizes.index(d['memtable_size'])
        j = unique_threads.index(d['thread_count'])
        Z[i, j] = d['ops_per_ms']
    
    # Create heatmap
    fig, ax = plt.subplots(figsize=(10, 8))
    im = ax.imshow(Z, cmap='viridis', aspect='auto', 
                   extent=[min(unique_threads), max(unique_threads), 
                          min(unique_memtable_sizes)/1000, max(unique_memtable_sizes)/1000],
                   origin='lower')
    
    # Set labels
    ax.set_xlabel('Thread Count')
    ax.set_ylabel('Memtable Size (k)')
    ax.set_title('Multi-threaded Memtable Performance Heatmap')
    
    # Add color bar
    cbar = fig.colorbar(im, ax=ax)
    cbar.set_label('ops/ms')
    
    # Set ticks
    ax.set_xticks(unique_threads)
    ax.set_yticks([x/1000 for x in unique_memtable_sizes])
    ax.set_yticklabels([f'{x/1000:.0f}k' for x in unique_memtable_sizes])
    
    # Add numerical labels on the heatmap
    for i in range(len(unique_memtable_sizes)):
        for j in range(len(unique_threads)):
            text = ax.text(unique_threads[j], unique_memtable_sizes[i]/1000, 
                          f'{Z[i, j]:.0f}', ha='center', va='center', 
                          color='white', fontweight='bold')
    
    plt.tight_layout()
    return fig, ax

def print_summary(data):
    """Print data summary"""
    print("=== Data Summary ===")
    print(f"Total data points: {len(data)}")
    
    if data:
        print(f"Thread count range: {min([d['thread_count'] for d in data])} - {max([d['thread_count'] for d in data])}")
        print(f"Memtable size range: {min([d['memtable_size'] for d in data])/1000:.1f}k - {max([d['memtable_size'] for d in data])/1000:.1f}k")
        print(f"Total OP count range: {min([d['total_op_count'] for d in data])/1000:.1f}k - {max([d['total_op_count'] for d in data])/1000:.1f}k")
        print(f"Maximum runtime range: {min([d['max_time'] for d in data])} - {max([d['max_time'] for d in data])} ms")
        print(f"Operations per millisecond range: {min([d['ops_per_ms'] for d in data]):.2f} - {max([d['ops_per_ms'] for d in data]):.2f} ops/ms")
    
    print("\n=== Detailed Data ===")
    for d in data:
        print(f"Thread Count: {d['thread_count']:2d}, "
              f"Memtable Size: {d['memtable_size']/1000:5.1f}k, "
              f"Total OPs: {d['total_op_count']/1000:6.1f}k, "
              f"Max Time: {d['max_time']:3d}ms, "
              f"Ops/ms: {d['ops_per_ms']:8.2f}")

def main():
    """Main function"""
    # Parse command line arguments
    parser = argparse.ArgumentParser(description='Visualize performance data from log files')
    parser.add_argument('logfile', nargs='?', default='data.log', 
                       help='Log file to process (default: data.log)')
    parser.add_argument('-o', '--output', default='performance',
                       help='Output filename prefix (default: performance)')
    parser.add_argument('--no-display', action='store_true',
                       help='Do not display charts (only save files)')
    
    args = parser.parse_args()
    filename = args.logfile
    output_prefix = args.output
    
    try:
        # Read data
        print(f"Reading data file: {filename}")
        data = read_data_log(filename)
        
        if not data:
            print("Error: No valid data read")
            return
        
        # Print data summary
        print_summary(data)
        
        # Create charts
        print("\nCreating 3D chart...")
        fig_3d, ax_3d = create_3d_plot(data)
        
        print("Creating 2D heatmap...")
        fig_2d, ax_2d = create_2d_heatmap(data)
        
        # Save charts
        output_3d = f"{output_prefix}_3d.png"
        output_heatmap = f"{output_prefix}_heatmap.png"
        
        fig_3d.savefig(output_3d, dpi=300, bbox_inches='tight')
        fig_2d.savefig(output_heatmap, dpi=300, bbox_inches='tight')
        print(f"Charts saved as {output_3d} and {output_heatmap}")
        
        # Display charts (unless --no-display is specified)
        if not args.no_display:
            plt.show()
        else:
            print("Charts saved. Use --no-display to skip display.")
        
    except FileNotFoundError:
        print(f"Error: File not found {filename}")
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main() 