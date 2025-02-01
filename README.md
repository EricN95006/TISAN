# TISAN
TIme Series Analysis eNvironment

Welcome to TISAN the TIme Series ANalyzer. See SOURCE/help.txt for more information on the program....

TISAN is intended to offer a quick and relatively easy way to process time series data. It can handle real or complex evenly spaced time series as well as real or complex time labeled files. What TISAN gives you is a playground to experiment with the application, exploration and integration of different techniques. You can also add your own algorithms to the list of tasks.

Install gcc. The code is written in C. It has been tested under MacOS and will likely work on any unix platform (except for changing the TISAN.CFG file to display BMP files). I expect there will be some surprises under Windows, which is a bit ironic since the original version was written for DOS.

There are five required folders inside the tisan folder:

	inputs
	run
	SOURCE
	SOURCE/SUPPORT
	data

The inputs and run folders are needed at run-time. The SOURCE folders are needed at compile time. The data folder has a sample data file used by the sample RUN file.

To compile the source files into executables you need gcc installed. Once you have a functioning compiler, in the terminal navigate to the SOURCE folder. From within the SOURCE directory make the executables using
	make clean
	make
	make index

You need to execute them in that order.

In the terminal change the directory to the tisan folder. From there execute ./tisan

If you want to run tisan directly by double clicking on it, there is a shell script called "Run TISAN CLI" that you can modify in any text editor (just drag it into one) to set the directory and execute the tisan app.

If you are on a Mac, at the TISAN prompt type

	run sample

and you will get a couple of plots in the Preview app in short order. On any other system you need to modify the TISAN.CFG text file to reflect how to bring up a BMP file in a viewer under software control prior to executing the sample run script.

The script converts a raw data file containing background radiation counts into a time labeled TISAN data file. It then plots it. A histogram is generated from that file and a Gaussian fit is made to it. The histogram with the fit are then plotted using wild cards in the file name.

There is another script called demo. That script generates a data file consisting of a sine wave and then applies various Fourier transforms to it. All the transforms are then plotted together, using wild cards in the file name extension.

To get help, type help at the TISAN prompt. You can also execute the help app from within a terminal window. You must be in the SOURCE directory to use that app.


If you want to add your own tasks to the system, start with either the candy.c or taffy.c templates, or with an existing task and then follow these steps to integrate it into the system.

Create an inputs index file
	In the SUPPORT folder create a TASKNAME.TXT file (all upper case with an eight character limit) that follows the pattern of the other inputs specification files to define the inputs used by your task.

Add the help text to help.txt
	Using the same structure as the other help entries, at the end of the file, put your text between two back-tics 
	The first line must be `TASKNAME: Description of the task
	The last line is just the second back-tic `

Add the task to the makefile
	Add the task name to the default: target before ./SUPPORT/addtask
	Add the compile directive for the new task before the Help Files
	Add the task to the index: target before the cp TISAN.IDX line
	Add the task to the clean: target

From within the SUPPORT directory make the executable
	make
	make index

You do not need to execute "make index" each time, unless you change the inputs or the addtask app.

You are now ready to use your new task within the TISAN environment.
