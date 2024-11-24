import pandas as pd
import matplotlib.pyplot as plt

csv_file = "load_test_results.csv"  
data = pd.read_csv(csv_file)

total_time = data["ThreadPoolSize"]
throughput = data["Throughput"]

plt.figure(figsize=(10, 6))
plt.plot(total_time, throughput, marker='o', linestyle='-', color='b', label='Throughput vs Threadpool size')

plt.xlabel("Threadpool size", fontsize=12)
plt.ylabel("Throughput (messages/second)", fontsize=12)
plt.title("Threadpool_size vs Throughput (Number of clients = 100)", fontsize=14)
plt.grid(True)
plt.legend()

output_file = "plot_1.png"  
plt.savefig(output_file, dpi=300, bbox_inches='tight') 
