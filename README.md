# tetris: A text-mode tetris game

Tetris is a simple game written in C, using NCurses for the terminal I/O.
It requires POSIX Thread and NCurses.

## Build and Run

To build tetris, run 'make' inside the directory where you have the source.
    ```shell
    $ make
    ```

To play the game, run the executable 'tetris'.
    ```shell	
    $ ./tetris
    ```

Key mapping:
  * Arrow Up: rotate the block
  * Arrow Down: drop the block
  * Arrow Left: move left
  * Arrow Right: move right
  * Q: Quit or Pause

## License
 
Tetris is released under the MIT license.
