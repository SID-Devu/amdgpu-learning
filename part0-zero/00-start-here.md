# Chapter 0 — Start here

This chapter has no code in it. Just words. **Read it slowly.** It will save you weeks of confusion later.

## You are not behind

Most people who join AMD, NVIDIA, or any chip company started exactly where you are. They were 22, fresh out of college, and the first time they opened the Linux kernel source they thought "I will never understand this." And then 6 months later they did.

The brain learns this stuff. It just takes repetition.

The trick is **don't quit when something is confusing.** Confusion is not a sign that you are stupid. Confusion is the feeling of your brain *building the model*. If you push through, the confusion clears. If you give up, it doesn't.

## The three rules

**Rule 1: Type, don't copy.**

When this book shows a command or a piece of code, you will type it yourself, character by character, into your terminal. No copy-paste. This feels slow. It is the fastest way to learn. Your fingers and your eyes need to see the same thing in two places before your brain remembers it.

**Rule 2: When something doesn't work, read the error message.**

90% of beginners' problems are because they didn't read the error. The computer literally tells you what is wrong. It might use words you don't know yet — that's OK, you will learn them. But it is *not* random. The error is the answer.

We will practice reading error messages in chapter 10.

**Rule 3: One thing at a time.**

You will see code that looks long and scary. Always **break it into small pieces**. Read one line. Understand it. Then the next. Don't try to understand a 50-line function at once.

## What "understanding" means

You don't need to memorize anything in this book. You need two things:

1. **Recognition** — "I have seen this before. I know roughly what it does. If I need details, I know where to look."
2. **Recall of the few core ideas** — Pointers. Memory. Pages. Locks. These you must internalize.

Everything else, you look up. **Senior engineers Google constantly.** They look up function signatures, header file paths, syscall numbers, register layouts. The job isn't to memorize. The job is to think clearly.

## What good vs bad looks like

**Bad day**: You read a chapter, understand 60% of it, feel frustrated, watch YouTube instead.

**Good day**: You read the chapter, understand 60% of it, write down the 3 things that confused you in your notebook, and move on. Tomorrow, after the next chapter, 2 of those 3 things make sense. The 3rd you ask in a forum or to me in another chat. By the end of the week the entire chapter clicks.

The goal is not to *not be confused*. The goal is to **stay calm while confused** and keep going.

## What "real-world" means in this book

Some books teach you toy programs that have no use anywhere. This book is different. Every example here is something you will actually see at AMD:

- The terminal commands in Chapter 2 are the ones you will run on your work laptop every day.
- The C programs in Chapter 9 use the same patterns the kernel uses.
- The debugging chapter (10) is exactly how senior engineers diagnose driver crashes.

You are not learning "school stuff." You are learning **the job.** Slowly, from the bottom.

## Try it (your first homework)

Right now, before chapter 1:

1. Get a paper notebook. Title the first page "Linux device drivers — daily log."
2. Write today's date. Write one sentence: "Today I started Part 0."
3. At the end of every study session, write 1–3 sentences:
   - what you learned
   - what confused you
   - what you'll try tomorrow

That notebook is going to be your most valuable tool for the next year. Don't skip it.

## A note about feeling lost

You will, at many points in this book, feel completely lost. That is the experience. Every single engineer at AMD has felt the same way at some point. Some of them still do, and they ship code anyway.

Lost is OK. Stuck is OK. **Quitting is not OK.**

When you feel lost, the answer is almost always one of:

- Re-read the previous chapter.
- Type out the code by hand again.
- Run the program with one small change to see what happens.
- Sleep. The next morning, it often clicks.

OK. Take a breath. Let's meet the computer.

➡️ Next: [Chapter 1 — What is a computer?](01-what-is-a-computer.md)
