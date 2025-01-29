#! /usr/bin/python3

from collections import defaultdict
import csv
import re

# Define the CSV file path
raw_file_path = '250128_benchmarks.csv'
processed_file_path = [
    '250128_benchmarks_add_int.csv',
    '250128_benchmarks_mul_int.csv',
    '250128_benchmarks_div_int.csv',
    '250128_benchmarks_add_rational.csv',
    '250128_benchmarks_mul_rational.csv',
    '250128_benchmarks_add_int_copy.csv',
    '250128_benchmarks_mul_int_copy.csv',
    '250128_benchmarks_div_int_copy.csv',
    '250128_benchmarks_add_rational_copy.csv',
    '250128_benchmarks_mul_rational_copy.csv',
]

pattern = [
    re.compile(r"Sum pairs integer ((MPL|GMP|CLN)(.*)) (\d+)"),
    re.compile(r"Sum mult integer ((MPL|GMP|CLN)(.*)) (\d+)"),
    re.compile(r"Sum ratio integer ((MPL|GMP|CLN)(.*)) (\d+)"),
    re.compile(r"Sum pairs rational ((MPL|GMP|CLN)(.*)) (\d+)"),
    re.compile(r"Sum mult rational ((MPL|GMP|CLN)(.*)) (\d+)"),
    re.compile(r"Sum pairs integer copy ((MPL|GMP|CLN)(.*)) (\d+)"),
    re.compile(r"Sum mult integer copy ((MPL|GMP|CLN)(.*)) (\d+)"),
    re.compile(r"Sum ratio integer copy ((MPL|GMP|CLN)(.*)) (\d+)"),
    re.compile(r"Sum pairs rational copy ((MPL|GMP|CLN)(.*)) (\d+)"),
    re.compile(r"Sum mult rational copy ((MPL|GMP|CLN)(.*)) (\d+)"),
]

data = [
    defaultdict(dict),
    defaultdict(dict),
    defaultdict(dict),
    defaultdict(dict),
    defaultdict(dict),
    defaultdict(dict),
    defaultdict(dict),
    defaultdict(dict),
    defaultdict(dict),
    defaultdict(dict),
]

with open(raw_file_path, 'r', newline='') as raw_file:
    raw_reader = csv.reader(raw_file)
    next(raw_reader)
    
    for row in raw_reader:
        [name, iterations, real_time, cpu_time, time_unit, bytes_per_second, items_per_second, label, error_occurred, error_message] = row
        for (pattern1, data1) in zip(pattern, data):
            try:
                (category, _, _, size) = pattern1.match(name).groups()
                data1[size][category] = real_time
            except AttributeError:
                pass

for (data, path) in zip(data, processed_file_path):
    with open(path, 'w', newline='') as processed_file:
        processed_writer = csv.writer(processed_file)
        keys = list(data.values())[0].keys()
        processed_writer.writerow(['Size', *keys])
        for (size, times) in data.items():
            processed_writer.writerow([size, *(times[key] for key in keys)])
