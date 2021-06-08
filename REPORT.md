# Project 4 Report

### Mounting and Unmounting

To mount the filesystem, we open the disk, load it and then read the superblock, FAT, and root directory info. We have the structures packed so that they're lined up in the correct block sizes so the the compiler doesnt interfere. Empty files and blocks are also reported. At unmounting, we write back changes we made during the running of the program to the disks metadata, then we close it. We mark the superblock closed so that we are able to open a new file system later. All file descriptors get closed before unmounting. This part of phase 1 was mostly straightforward. Most of it was spent thinking of ways to put in more bounds checks for corner cases and out of bounds testing, that may not be in the provided testers. 

### File Creation and Deletion

To create, we check the root directory to make sure such a filename doesn't already exist,then find an empty spot to create a new file object. When deleteting, we check to make sure the file is valid and exists in the root directory. We then mark it as deleted by using memset and erasing the information. 

### Opening and Closing
Everything up to including phase 3 was by instruction. At this point in order to bypass complicated pointering and arrowing we wrote a static function here to help us. open_fd(), which opens a files descriptors ID, which we then assign to a variable that gets returned when we call fs_open. It was at this point we also figured out that for read and write that we would need to write another static helper functions. rd_find() which searches our root directory for a filename and returns it to us. We debated whether embedded static functions that would call each other would be more modular and extendable, potentially making it that we would need less code lines total. But having each one do an individual action instead of say having one that acceses filenames, and through then, be called to access, sizes, and then through that, to access if its appropriate descriptor were open, would be more difficult to read. 


### Reading and Writing
This is where most of the work was done and where implementation got more complicated. The main coding issue was that for write there was development trouble allocating the correct amount of data blocks for the file making structure that writes big files that span several data blocks iteratively. There were issues adjusting for the right offset amount while going through the FAT block entires. The creation of some type of buffer in theory was needed for the writing portion. Since we can't write directly to the block we have to put it in a buffer and write to it , allowing us to in a round about way write to the file system. We also had to track the offset in the buffer and the eventual differences in size and potentail other metadata when updating the file in the directory entries. This was just incredibly time consuming and invovled a lot of one off adjustments and changing the order of functions that we were called. Also we had to place updates to a variety of things happening in different tables, lists and so forth, checking each time an action was made, what would need to be changed. It just had to be very precise and took a lot of time. 


