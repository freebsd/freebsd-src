#ifndef OUT2CON_H
#define OUT2CON_H

/* Call CreateConsoleEcho() to create a console and begin echoing stdout to it.
 * The original stream (if any) will still receive output from stdout.
 * Call DestroyConsoleEcho() to stop echoing stdout to the console.
 * The original stream continues to receive stdout.
 *
 * WARNING: it is not safe to use stdout from another thread during
 *          CreateConsoleEcho() or DestroyConsoleEcho()
 */

class ConsoleEcho;

ConsoleEcho *
CreateConsoleEcho();

void
DestroyConsoleEcho(ConsoleEcho *consoleEcho);

// Convenience class to automatically echo to console within a scope
class AutoConsoleEcho
{
public:
    AutoConsoleEcho() : m_echo(CreateConsoleEcho())
    {
    }

    ~AutoConsoleEcho()
    {
        DestroyConsoleEcho(m_echo);
    }
private:
    ConsoleEcho* m_echo;
};


#endif
