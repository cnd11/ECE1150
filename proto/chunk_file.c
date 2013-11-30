#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#define CHUNK_BYTE_SIZE "7"    // Number of bytes for each chunk

int main( int argc, char **argv )
{
  char *file_name = argv[1];
  pid_t chunk_proc;
  char *time_str;

  if( argc < 2 )
    {
      printf( "Requires file name for chunking! ./chunk_file file.ext\n" );
      exit( EXIT_FAILURE );
    }

  if( !(access(file_name, F_OK) == 0 && access(file_name, R_OK) == 0) )
    {
      perror("File doesn't exist or no read permissions!");
      exit( EXIT_FAILURE );
    }

  time_str = malloc( sizeof(time_t) );
  sprintf( time_str, "%ld", time(NULL) );

  switch( (chunk_proc = fork()) )
    {

    case -1:    // Fork error case. Returns pid < 0
      free( time_str );

      perror( "Unable to fork child process!\n");
      exit( EXIT_FAILURE );
      break;

    case 0:    // Child process case
      {
	/*
	  Runs shell command using execlp. Split command to split file into
	  chunks of b Bytes. Resulting files have the current time in seconds + XX.

	  EX: split -b 14 file.txt 123456789 ->
	              1234456789aa 123456789ab 123456789ac ...
	  
	 */

	execlp( "split", "split", "-b", CHUNK_BYTE_SIZE , file_name, time_str,  NULL);
	break;
      }

    default:  // Parent process case 
      {
	break;
      }
    }

  free( time_str );
      
  return EXIT_SUCCESS;
}
