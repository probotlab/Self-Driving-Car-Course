import cv2
import os

# Folder containing images
image_folder = r"C:\VS Code Codes\Self Driving Car\12 Laps perfected new\backward_images"
output_video = r"C:\VS Code Codes\Self Driving Car\12 Laps perfected new\backward_video.avi"

# Get sorted list of JPG images
images = [img for img in os.listdir(image_folder) if img.endswith(".jpg")]
images.sort()

# Read first image to get dimensions
first_image_path = os.path.join(image_folder, images[0])
frame = cv2.imread(first_image_path)
height, width, _ = frame.shape

# Set up video writer
fourcc = cv2.VideoWriter_fourcc(*'XVID')
fps = 24.0
video = cv2.VideoWriter(output_video, fourcc, fps, (width, height))

# Font settings
font = cv2.FONT_HERSHEY_SIMPLEX
font_scale = 1
font_color = (0, 255, 0)  # Green
thickness = 2

for image_name in images:
    image_path = os.path.join(image_folder, image_name)
    frame = cv2.imread(image_path)

    # Extract steering angle from filename (second element after splitting by "_")
    parts = image_name.split("_")
    if len(parts) >= 2:
        steering_angle = parts[1]
    else:
        steering_angle = "N/A"

    # Prepare text
    text = f"Steering: {steering_angle}"

    # Get text size to position in top-right
    (text_width, text_height), _ = cv2.getTextSize(text, font, font_scale, thickness)
    position = (width - text_width - 10, text_height + 10)  # Top-right corner

    # Overlay text on image
    cv2.putText(frame, text, position, font, font_scale, font_color, thickness)

    # Write frame to video
    video.write(frame)

video.release()
print(f"Video saved to: {output_video}")
