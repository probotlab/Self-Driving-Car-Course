import os
import shutil

# Source folder
source_folder = r"C:\VS Code Codes\Self Driving Car\tryout_backward_1"
# Destination folder for unique images
destination_folder = os.path.join(source_folder, "unique_images")

# Create destination folder if it doesn't exist
os.makedirs(destination_folder, exist_ok=True)

# Set to track unique image numbers
unique_numbers = set()

# Process files in the source folder
for filename in os.listdir(source_folder):
    if filename.lower().endswith(".jpg"):
        # Extract the first part before the first underscore
        image_number = filename.split('_')[0]

        if image_number not in unique_numbers:
            unique_numbers.add(image_number)
            source_path = os.path.join(source_folder, filename)
            dest_path = os.path.join(destination_folder, filename)
            shutil.copy(source_path, dest_path)

print(f"Copied {len(unique_numbers)} unique images to: {destination_folder}")
