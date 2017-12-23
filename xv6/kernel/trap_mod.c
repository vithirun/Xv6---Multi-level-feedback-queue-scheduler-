#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "proc.c"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
static struct proc *p;
static struct ptable ptable;

uint ticks;

	void
tvinit(void)
{
	int i;

	for(i = 0; i < 256; i++)
		SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
	SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

	initlock(&tickslock, "time");
}

	void
idtinit(void)
{
	lidt(idt, sizeof(idt));
}

	void
trap(struct trapframe *tf)
{
	if(tf->trapno == T_SYSCALL){
		if(proc->killed)
			exit();
		proc->tf = tf;
		syscall();
		if(proc->killed)
			exit();
		return;
	}

	switch(tf->trapno){
		case T_IRQ0 + IRQ_TIMER:
			if(cpu->id == 0){
				acquire(&tickslock);
				ticks++;
				wakeup(&ticks);
				release(&tickslock);
			}
			lapiceoi();
			break;
		case T_IRQ0 + IRQ_IDE:
			ideintr();
			lapiceoi();
			break;
		case T_IRQ0 + IRQ_IDE+1:
			// Bochs generates spurious IDE1 interrupts.
			break;
		case T_IRQ0 + IRQ_KBD:
			kbdintr();
			lapiceoi();
			break;
		case T_IRQ0 + IRQ_COM1:
			uartintr();
			lapiceoi();
			break;
		case T_IRQ0 + 7:
		case T_IRQ0 + IRQ_SPURIOUS:
			cprintf("cpu%d: spurious interrupt at %x:%x\n",
					cpu->id, tf->cs, tf->eip);
			lapiceoi();
			break;

		default:
			if(proc == 0 || (tf->cs&3) == 0){
				// In kernel, it must be our mistake.
				cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
						tf->trapno, cpu->id, tf->eip, rcr2());
				panic("trap");
			}
			// In user space, assume process misbehaved.
			cprintf("pid %d %s: trap %d err %d on cpu %d "
					"eip 0x%x addr 0x%x--kill proc\n",
					proc->pid, proc->name, tf->trapno, tf->err, cpu->id, tf->eip, 
					rcr2());
			proc->killed = 1;
	}

	// Force process exit if it has been killed and is in user space.
	// (If it is still executing in the kernel, let it keep running 
	// until it gets to the regular system call return.)
	if(proc && proc->killed && (tf->cs&3) == DPL_USER)
		exit();

	// Force process to give up CPU on clock tick.
	// If interrupts were on while locks held, would need to check nlock.
	if(proc && proc->state == RUNNING && tf->trapno == T_IRQ0+IRQ_TIMER)
	{
		proc->p_ticks[proc->queue]++;
		 acquire(&ptable.lock);
		 for(p = &ptable.proc; p < &ptable.proc[NPROC]; p++)
                                {
                                        if(p->queue == proc->queue)
                                                p->p_wait_ticks[p->queue]++;
				}
		release(&ptable.lock);

	// cprintf("\ntimer tick has occurred!!\nprocess: %s pid: %d state: %s queue: %d ticks completed: %d\n",proc->name, proc->pid, states[proc->state], proc->queue, proc->timer_ticks[proc->queue]);

	switch(proc->queue)
	{
		case 3:
			if(proc->p_ticks[proc->queue] % 8 == 0)
			{
				// cprintf("\nstopping and demoting!!\nprocess: %s pid: %d state: %s queue: %d -> %d ticks completed: %d\n",proc->name, proc->pid, states[proc->state], proc->queue, proc->queue+1, proc->timer_ticks[proc->queue]);
				proc->queue--;
				yield();
			}
			break;
		case 2:
			if(proc->p_ticks[proc->queue] % 16 == 0)
			{
				// cprintf("\nstopping and demoting!!\nprocess: %s pid: %d state: %s queue: %d -> %d ticks completed: %d\n",proc->name, proc->pid, states[proc->state], proc->queue, proc->queue+1, proc->timer_ticks[proc->queue]);
                                        if(proc->p_wait_ticks[proc->queue]>=160)
                                                {
                                                        proc->p_wait_ticks[proc->queue]=0;
                                                        proc->queue++;
                                                }

				proc->queue--;
				yield();
			}
			break;
		case 1:
			if(proc->p_ticks[proc->queue] % 32 == 0)
			{
				// cprintf("\nstopping and demoting!!\nprocess: %s pid: %d state: %s queue: %d -> %d ticks completed: %d\n",proc->name, proc->pid, states[proc->state], proc->queue, proc->queue+1, proc->timer_ticks[proc->queue]);
				if(proc->p_wait_ticks[proc->queue]>=320)
                                                {
                                                        proc->p_wait_ticks[proc->queue]=0;
                                                        proc->queue++;
                                                }
					proc->queue--;
				yield();
			}
			break;
		case 0:
			if(proc->p_wait_ticks[proc->queue]>=500)
                                                {
                                                        proc->p_wait_ticks[proc->queue]=0;
                                                        proc->queue++;
                                                }
			break;
	}

}

// Check if the process has been killed since we yielded
if(proc && proc->killed && (tf->cs&3) == DPL_USER)
	exit();
	}
