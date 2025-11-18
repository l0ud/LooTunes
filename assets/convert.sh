#!/bin/bash

INPUT_DIR="./in" # change this to your source directory
OUTPUT_DIR="./out"  # change this to output directory

mkdir -p "$OUTPUT_DIR"

for file in "$INPUT_DIR"/*.flac; do
    [ -e "$file" ] || continue  # skip if no FLAC files exist
    filename=$(basename -- "$file")
    output_file="$OUTPUT_DIR/${filename%.flac}.sbc"
    
    echo "Converting: $file -> $output_file"
    ffmpeg -n -i "$file" -ac 2 -c:a sbc -b:a 328k "$output_file"
done

for file in "$INPUT_DIR"/*.mp3; do
    [ -e "$file" ] || continue  # skip if no MP3 files exist
    filename=$(basename -- "$file")
    output_file="$OUTPUT_DIR/${filename%.mp3}.sbc"
    
    echo "Converting: $file -> $output_file"
    ffmpeg -n -i "$file" -ac 2 -c:a sbc -b:a 328k "$output_file"
done

echo "All files converted!"
