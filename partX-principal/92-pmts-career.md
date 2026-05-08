# Chapter 92 — The PMTS career arc — the 24-year view

You are starting now. The 24-year PMTS you're competing with started in 2002. You can't compress that into 2 years. But you can plan your next 24 with intention, so when you're year 12, year 18, year 24, you're better than they were at the same year.

This is the closing chapter. It's about the long game.

## The shape of a career

```
Years 0-1     :  fresh hire / contractor — competent on tools
Years 1-3     :  competent junior — ships features, fixes bugs reliably
Years 3-5     :  senior — owns a feature area; reviewer trusted by team
Years 5-8    :  staff — leads cross-team feature; respected on mailing lists
Years 8-12   :  principal — architectural decisions; mentors staff engineers
Years 12-20  :  PMTS / Distinguished — sets multi-year direction; speaks at major conferences
Years 20+    :  Fellow / VP-IC track — industry-shaping; few people in this seat
```

These titles vary by company. AMD ladder: Engineer I → II → III → MTS (Senior) → SMTS → PMTS → Fellow. Many MTSes are excellent; few become PMTS.

## What changes at each level

### 0–1 year

You're being onboarded. Your tools are wobbly. Your knowledge of AMD GPUs is from this book (or worse). Your job is to **become a useful contributor**: ship 5–10 small patches; learn the tools; build relationships.

### 1–3 years

You ship features. You debug things others can't. You start to be the person juniors come to. You **specialize** — pick one area (GPUVM, RBE, VCN, KFD, display, SMU, ...) and own it. Read every patch in that area, even if you aren't the author. Become the person whose `Reviewed-by:` matters.

### 3–5 years (senior)

You can land features without anyone holding your hand. You own design discussions. You handle escalations from customers. You become an upstream maintainer for a sub-area. You start mentoring juniors actively.

The transition from competent-junior to senior is **trust + ownership**: you're handed a hard bug, and you don't need follow-up emails. You finish.

### 5–8 years (staff)

You're now influencing things outside your immediate team. You're at planning meetings for the next chip. Your name is on industry talks. You spend more time on architecture and review than on writing. You **multiply** other engineers' output.

The transition is from "I do hard things" to "I make my whole team productive."

### 8–12 years (principal)

You decide multi-year directions for AMD. "Should we invest in mesh shaders or virtual geometry?" "Should we deprecate API X or keep maintaining it?" You sit in rooms with VPs and CTOs. You write strategy documents. You're sometimes the AMD voice at standards bodies.

You may write less code than you used to. You may miss it. The trade-off was inevitable.

### 12–20 years (PMTS, Distinguished)

Few people reach here. You are AMD's spokesperson on a topic — the person customers, press, and standards bodies believe. You **shape** the industry through papers, talks, code, opinions. Your career compounds: every year you build more pull on what AMD does.

### 20+ years (Fellow, VP-IC track)

Only a handful of seats. You're the technical conscience of the company. You can spend years on one big bet because the company trusts your judgment. Your retirement marks the end of an era.

## What kills a career at each stage

**0–1**: not asking for help; faking understanding; bouncing between teams without depth.

**1–3**: not specializing; being a generalist forever; complaining about company politics instead of shipping.

**3–5**: not learning to mentor; not learning to communicate beyond the technical; not building reputation upstream.

**5–8**: not engaging with cross-team / cross-org problems; staying in one team forever; still wanting to write all the code yourself.

**8–12**: bad hires (you're hiring now, and bad hires are your fault); refusing to delegate; not making time for strategy.

**12+**: living in the past architectures; not learning new domains; not raising the next generation.

## The compounding investments

Every year, do at least one of these. They compound.

1. **Write something durable** — a patch series, a paper, a blog post, a tool.
2. **Mentor one junior** — actively, weekly, with patience.
3. **Speak publicly once** — internal lunch-and-learn, conference, university recruiting.
4. **Read deeply outside your area** — reading a competitor's whitepaper is "outside your area" the first 5 years.
5. **Build a relationship with a customer or partner** — a name you can email anytime.

After 5 years of one-each-of-these, you have:

- 5 durable artifacts (people quote you).
- 5 engineers you trained (some now senior).
- 5 talks given (you're "known" in the niche).
- Broad cross-vendor literacy.
- A small network of customers who trust you.

Compound that for 20 years. That is what a PMTS *is*.

## The non-technical skills

You will be evaluated for promotion on:

- Technical depth (yes, you knew this).
- **Influence**: do others change behavior because of you?
- **Communication**: written, spoken, presentation.
- **Judgment**: do your design decisions hold up over time?
- **Mentorship**: are juniors you work with getting better?
- **Reliability**: when you commit, does it land?

You can be the smartest person in the room and not get promoted to PMTS if your reliability is poor or you can't mentor.

## Money, family, location

Practical realities for a long career:

- **Compensation**: PMTS-level total comp is high — hundreds of thousands USD/year with stock — but it's earned, not handed. Plan accordingly. Don't take golden-handcuff risks early; take career-defining risks early.
- **Family**: 24 years includes marriage, children, parental care. Those are not distractions; they are the reason. Work-life integration evolves. Companies that demand round-the-clock work eat their seniors.
- **Location**: AMD has offices in Markham (Toronto), Austin, Bangalore, Shanghai, Munich. Some teams are remote-first. Where you live affects which customers you talk to.
- **Health**: a sustained sprint of code-review-debug-mentor without exercise/sleep ends in burnout by year 8. The PMTS engineers I know who are still strong at year 24 made this investment early.

## The horrible truth: not everyone makes PMTS

The pyramid is real. Most engineers stop at MTS or SMTS. That's still a great career. The leap to PMTS requires:

- Right-time-right-place (a chip cycle where your area exploded).
- Right manager (one who advocates for you).
- Right company (one that has a PMTS-level role open).
- Right portfolio of impact.

You can do everything right and not make it. **Don't make PMTS the only goal.** Make the *journey* the goal. The engineering you do, the people you mentor, the bugs you fix, the customers you delight — those are the value.

## A 24-year plan, sketched

| Year | Goal |
|---|---|
| Year 0–1 | Land 10+ patches in `drivers/gpu/drm/amd/`. Convert from contractor to FTE. Pick a specialty. |
| Year 1–2 | Become the team's GPUVM person (or RBE, or VCN, or…). One conference talk. |
| Year 2–4 | Senior promotion. First mentee. Co-author a blog post. |
| Year 4–6 | Lead a feature spanning 3+ files. Start reviewing patches outside your area. |
| Year 6–8 | Staff promotion. Chair a standards working group sub-feature. Mentor 2–3 juniors. |
| Year 8–10 | Cross-org influence. New chip's bring-up effort co-led by you. |
| Year 10–14 | Principal promotion. Architectural decisions on next-gen. |
| Year 14–18 | PMTS push. Mature mentor of staff engineers. Industry-known. |
| Year 18–24 | Fellow track or VP-IC track. Decide. Either is OK. |

Adjust everything by ±2-4 years for realities of life.

## The alternative paths

You don't have to climb to PMTS to have a great life:

- **People-management**: lead a team of engineers; lead an org. Different skills; equally rewarded.
- **Founder / startup**: if AMD's ladder isn't fast enough, the next great GPU/AI company hasn't been founded yet.
- **Academia**: PhD at year 5, become a professor. Different reward function (papers, students).
- **Consulting**: senior expert with their own clients, no employer.

Don't let "I should be a PMTS" trap you in a path that doesn't fit you. Audit yourself yearly: am I still excited?

## A final word

The 24-year PMTS you're competing with knew nothing in their year 0. They became what they are by 24 years of attention. **You can do the same.** It's not about IQ; it's about iteration. Show up; do good work; ship; mentor; repeat.

The book ends here. The career begins.

Welcome to it.

---

*"I do not know what I may appear to the world; but to myself I seem to have been only like a boy playing on the seashore, and diverting myself in now and then finding a smoother pebble or a prettier shell than ordinary, whilst the great ocean of truth lay all undiscovered before me." — Isaac Newton.*
