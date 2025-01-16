#! /usr/bin/python3

from collections import defaultdict
import csv
import re

# Define the CSV file path
raw_file_path = 'sum_rationals_benchmark_results_raw.csv'
processed_file_path = 'sum_rationals_benchmark_results_processed.csv'

pattern = re.compile(r"Sum squares rational (.*) (\d+)_mean")

data = defaultdict(dict)

with open(raw_file_path, 'r', newline='') as raw_file:
    raw_reader = csv.reader(raw_file)
    next(raw_reader)
    
    for row in raw_reader:
        [name, iterations, real_time, cpu_time, time_unit, bytes_per_second, items_per_second, label, error_occurred, error_message] = row
        match_ = pattern.match(name)
        if match_:
            (category, size) = match_.groups()
            data[size][category] = real_time

with open(processed_file_path, 'w', newline='') as processed_file:
    processed_writer = csv.writer(processed_file)
    keys = list(data.values())[0].keys()
    processed_writer.writerow(['Size', *keys])
    for (size, times) in sorted(data.items(), key = lambda x: int(x[0])):
        processed_writer.writerow([size, *(times[key] for key in keys)])
