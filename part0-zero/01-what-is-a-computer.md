# Chapter 1 — What is a computer? (in plain English)

We will eventually talk about caches, page tables, MMIO, and DMA. **Not today.** Today we just learn what a computer *is*, in everyday words.

## The kitchen analogy

Imagine a small kitchen with one cook. The cook can do simple things very fast: chop, stir, taste.

| Kitchen | Computer |
|---|---|
| The **cook** | The **CPU** (the thing that does work) |
| The **counter** in front of the cook | **RAM** (memory you can use *right now*) |
| The **fridge / pantry** | The **disk** (storage that survives even when off) |
| The **recipe** the cook is following | The **program** |
| **Ingredients** on the counter | **Data** in memory |

Three rules of this kitchen:

1. The cook can only work with what's on the counter. If an ingredient is in the fridge, somebody has to bring it to the counter first. (CPUs only operate on what's in RAM, or even closer — in registers/caches.)
2. The counter is small. The fridge is huge. (RAM is GBs; disk is hundreds of GBs to TBs.)
3. The cook is *very* fast. Going to the fridge is *very* slow compared to looking at the counter. (RAM is ~100 times slower than the CPU's tiny on-chip caches; disk is ~100,000 times slower than RAM.)

Most performance problems in computers come from rule 3.

## The five parts you should know by name

```
                ┌──────────────────────┐
                │       CPU            │   does work
                │  (cook)              │
                └──────────────────────┘
                          │
                ┌──────────────────────┐
                │       RAM            │   short-term workspace
                │   (counter)          │
                └──────────────────────┘
                          │
                ┌──────────────────────┐
                │       Disk           │   long-term storage
                │   (fridge)           │
                └──────────────────────┘
                          │
            ┌──────────────────────────────┐
            │  Other devices               │
            │  GPU, NIC, USB, screen, ...  │
            └──────────────────────────────┘
                          │
                ┌──────────────────────┐
                │  You (human)         │
                │  keyboard, mouse     │
                └──────────────────────┘
```

That's it. That's a computer. Everything else is detail.

## "What is the GPU then?"

The GPU is *another* worker. Imagine the cook has a friend in the same kitchen — a friend with **many hands** who can do simple things in parallel: chop 64 carrots at once, but can't read complicated recipes alone.

| | CPU | GPU |
|---|---|---|
| Number of "workers" | 4–32 fast cores | thousands of small cores |
| Good at | branchy logic, anything | doing the same math on lots of data |
| Has its own memory? | uses system RAM | usually has **VRAM** (its own RAM) |
| Talks to it via | direct CPU instructions | "send a recipe over a cable" (PCIe) |

The CPU writes a recipe ("multiply these 4 million numbers by 2") on a piece of paper, hands it to the GPU through a slot in the wall (the **PCIe** cable), and goes off to do other things. The GPU does the math, hands the result back through the slot.

That "writing the recipe" + "passing it through the slot" + "telling the GPU when to start" is what an **AMD GPU driver** does. That's your future job in a single sentence.

## Where Linux fits in

Imagine the kitchen has many cooks who all want to use the same counter, the same fridge, the same friends-with-many-hands. They can't all just grab things, or chaos. So there is a **head chef** who:

- decides who gets the counter when
- protects the fridge so nobody steals
- keeps every cook from interfering with the others
- talks to the friends-with-many-hands on behalf of every cook

That head chef is the **operating system**. **Linux** is one head chef. **Windows** and **macOS** are other head chefs. They all do roughly the same job, in different ways.

A **device driver** is a part of the head chef that knows how to talk to one specific friend. The amdgpu driver knows how to talk to AMD's GPUs.

## The screen, the keyboard, and "everything is a device"

When you press a key on your keyboard:

1. The keyboard hardware sends a tiny electrical signal.
2. A driver inside Linux (the keyboard/USB driver) catches the signal and turns it into "the user pressed the letter A."
3. Linux passes that information to whatever program you're typing into.

When a program wants to draw a window:

1. The program asks Linux to draw something.
2. Linux passes the request to the GPU driver.
3. The GPU driver turns it into instructions the GPU can run.
4. The GPU does the drawing into a piece of memory.
5. The display hardware reads that memory and lights up the pixels you see.

This is the *whole* model. Everything we do for the rest of the book is filling in details about each step.

## "But what is *inside* the CPU?"

Just enough to know:

- The CPU has a small set of named slots called **registers** — like cubbies on the cook's apron. Each holds one number. Reading from a register is the fastest possible thing the CPU does.
- It also has tiny, fast **caches** (called L1, L2, L3) — basically a magnetic strip *on the counter itself* that holds copies of recently-used ingredients so the cook doesn't keep walking to the fridge.

That's all you need for now. We'll come back to caches in Part VIII when it actually matters.

## "What about the cloud, the internet, AI, …"

A "server" is just a computer in someone else's room. The "cloud" is just lots of those rooms. The "internet" is wires connecting rooms.

When you watch a YouTube video: the video file is on some server's *disk*, sent over the *network* to your computer's RAM, decoded by *your CPU and GPU*, drawn to *your screen*. Same five parts. Just two computers connected by a wire.

It's all the same picture, made bigger.

## Try it (5 minutes, no code yet)

In your notebook, draw the diagram from this chapter: CPU, RAM, disk, GPU, you. Label every box. Draw arrows for "data goes this way" between them.

Don't peek at the chapter while you do it. Drawing it from memory is the point.

If you get it wrong, that's fine — flip back, find what was wrong, and re-draw.

## Self-check

- What's the difference between RAM and disk? (Hint: counter vs fridge.)
- Why is going from CPU to disk slow? (Hint: distance and design — it's not about cables, it's about the kind of memory.)
- Why does a GPU need a special driver and a regular keyboard doesn't, sort of?
  (Trick question — the keyboard does have a driver. But the GPU's driver is way more complex because the GPU is a whole *other computer* that takes instructions, while a keyboard just sends key codes.)

You now know what a computer is. Tomorrow we will actually open one.

➡️ Next: [Chapter 2 — Using Linux: the terminal](02-using-linux.md)
