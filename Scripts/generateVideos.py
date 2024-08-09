#Generate a video from a set of images on each subfolder of a given folder

import os
import sys
import cv2
import numpy as np
import glob
import argparse
import shutil
import subprocess
import time
from tqdm import tqdm

# The parent directory of this file location + /Screenshots
dir_path = os.path.dirname(os.path.realpath(__file__)) + "/../Screenshots"
dir_path = os.path.normpath(dir_path)
print("Looking for images in dir_path: " + dir_path)
output_dir_path = "C:/Users/JOSE MANUEL/Desktop/Prácticas3dperceptionlab/videos"
#output_dir_path = "C:/Users/david/Desktop/UnrealFallDev/UnrealFall/Videos"

def generateVideoFromImages(folder, outputFolder, fps, width, height, extension):
    #print("Generating video from images in folder: " + folder)
    #print("Output folder: " + outputFolder)
 
    if not os.path.exists(outputFolder):
        os.makedirs(outputFolder)

    for subfolder in os.listdir(folder):
        subfolderPath = os.path.join(folder, subfolder)
        if os.path.isdir(subfolderPath):
            print("Exploring subfolder: " + subfolder)
            #Check if there are more subfolders inside
            generateVideoFromImages(subfolderPath, subfolderPath, fps, width, height, extension)
            
            images = glob.glob(subfolderPath + "/*." + extension)
            images.sort()
            if len(images) > 0:
                print("Generating video from images in folder: " + subfolderPath)
                print("Number of images: " + str(len(images)))
                
                #Save in output folder with the name format: subfolder-3_subfolder-2_subfolder-1_subfolder.avi, in avi format
                #Get the name of the subfolders and concatenate them
                folders = subfolderPath.split("\\")
                print("Folders: " + str(folders))
                #Join folders -from the third to the last- with _
                scene = folders[-4].split("_")[0].replace("-", "_")
                
                videoPath = output_dir_path + "/" + scene +"_" + "_".join(folders[-3:]) + ".avi"
                
                print("Video path: " + videoPath)
                
                #Generate video in avi
                fourcc = cv2.VideoWriter_fourcc(*'MJPG')
                video = cv2.VideoWriter(videoPath, fourcc, fps, (width, height))
                for image in tqdm(images):
                    #print("Adding image: " + image)
                    img = cv2.imread(image)
                    video.write(img)
                video.release()
                
                
                #fourcc = cv2.VideoWriter_fourcc(*'mp4v')
                #video = cv2.VideoWriter(videoName, fourcc, fps, (width, height))
                #for image in tqdm(images):
                #    #print("Adding image: " + image)
                #    img = cv2.imread(image)
                #    video.write(img)
                #video.release()
                #print("Video generated: " + videoName)
            else:
                print("No images found in folder: " + subfolderPath)
        else:
            #print("Not a folder: " + subfolderPath)
            pass
            




fps = 30
width = 1920
height = 1080
extension = "png"


print("FPS: " + str(fps))
print("Width: " + str(width))
print("Height: " + str(height))
print("Extension: " + extension)
    
generateVideoFromImages(dir_path, dir_path, fps, width, height, extension)


#for each animation folder
#for animationFolder in animationFolders:
#    generateVideoFromImages(dir_path + "/" + animationFolder, dir_path + "/" + animationFolder + "/videos", 30, 1920, 1080, "png")