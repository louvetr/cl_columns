gcc cl_columns.c $(ncursesw5-config --cflags) -c -gdwarf -g -ggdb -Wall 

gcc cl_columns.o $(ncursesw5-config --libs) -o cl_columns -gdwarf -g -ggdb -Wall
