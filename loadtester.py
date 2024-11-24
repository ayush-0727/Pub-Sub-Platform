import time
import subprocess
import random
import csv
from threading import Thread

MIN_DELAY = 0.5
MAX_DELAY = 2.0
NUM_TOPICS = 5 
TOPICS = [f"Topic_{i}" for i in range(NUM_TOPICS)]  
NUM_PUBLISHERS = 50
NUM_SUBSCRIBERS = 50
MESSAGES_PER_PUBLISHER = 20

LOG_FILE = "load_test_results.csv"

def run_instance(executable, inputs):
    process = subprocess.Popen(
        [executable],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    try:
        for action in inputs:
            process.stdin.write(action)
            process.stdin.flush()
            time.sleep(random.uniform(MIN_DELAY, MAX_DELAY))
        if executable == "./publisher":
            process.stdin.write("3\n")  
        else:   
            process.stdin.write("4\n") 
        process.stdin.flush()
        stdout, stderr = process.communicate(timeout=10)
        return stdout, stderr
    except subprocess.TimeoutExpired:
        process.kill()
        return None, None

def publisher_task(index):
    topic = random.choice(TOPICS)
    inputs = [f"1\n{topic}\n"] + [f"2\n{topic}\nMessage_{index}_{i}\n" for i in range(MESSAGES_PER_PUBLISHER)]
    run_instance("./publisher", inputs)

def subscriber_task(index):
    inputs = [f"1\n{random.choice(TOPICS)}\n", "2\n"] * 5 
    run_instance("./subscriber", inputs)

def simulate_load_test(thread_pool_size):
    start_time = time.time()  

    publisher_threads = []
    for i in range(NUM_PUBLISHERS):
        thread = Thread(target=publisher_task, args=(i,))
        publisher_threads.append(thread)
        thread.start()

    subscriber_threads = []
    for i in range(NUM_SUBSCRIBERS):
        thread = Thread(target=subscriber_task, args=(i,))
        subscriber_threads.append(thread)
        thread.start()

    for thread in publisher_threads + subscriber_threads:
        thread.join()

    end_time = time.time()
    total_time = end_time - start_time
    total_messages = NUM_PUBLISHERS * MESSAGES_PER_PUBLISHER + NUM_SUBSCRIBERS * 10
    throughput = total_messages / total_time

    with open(LOG_FILE, "a", newline="") as file:
        writer = csv.writer(file)
        writer.writerow([thread_pool_size, total_messages, total_time, throughput])

    print(f"Thread Pool Size: {thread_pool_size}, Throughput: {throughput:.2f} messages/sec")

if __name__ == "__main__":
    with open(LOG_FILE, "w", newline="") as file:
        writer = csv.writer(file)
        writer.writerow(["ThreadPoolSize", "TotalMessages", "TotalTime", "Throughput"])

    simulate_load_test(50)
