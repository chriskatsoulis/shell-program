Shell Program
========

Shell Program implements a large subset of the POSIX Shell’s functionality, including:

   - Parse command-line input into commands to be executed
   - Execute a variety of external commands (programs) as separate processes
   - Implement a variety of shell built-in commands within the shell itself
   - Perform a variety of i/o redirection on behalf of commands to be executed
   - Assign, evaluate, and export to the environment, shell variables
   - Implement signal handling appropriate for a shell and executed commands
   - Manage processes and pipelines of processes using job control concepts

The provided makefile was used to build Shell Program.
 
.. code-block:: console

   $ make          # Equivalent to `make all`
   $ make all      # Equivalent to `make release debug` (default target)
   $ make release  # Release build in release/ -- no debugging messages
   $ make debug    # Debug build in debug/ -- includes assertions and debugging messages
   $ make clean    # Removes build files (release/ and debug/ directories)

A reference implementation is provided in ``reference/shell``.
