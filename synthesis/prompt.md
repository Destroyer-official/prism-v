Master Prompt: Next-Gen Android Video Compression (Lossless + Lossy), 2027+
Nov 14, 2026 · @shantanu
Write a complete, detailed implementation plan for the best video compression system on Android for 2027 and beyond. This is a planning task only: do not write application code and do not build an APK. The plan will guide the build later.
Goal
Offer two modes that the user chooses between. Aim for the best system possible, not a copy of what already exists.

- Lossless: bit-exact output, as small as possible.
- Lossy: the best quality per bit, with the user choosing the target (quality, file size, speed or battery use).
In both modes, use as little memory, CPU, GPU/NPU, power and time as possible, on phones from low-end to flagship. You decide how to balance these goals and how to let the user pick the trade-off.
Plan for phones, chips and standards expected in 2027–2030. State your assumptions about them, and say what changes if they are wrong.
Your freedom
You decide every technical choice. Choose what to research, what to invent, which existing techniques to use, combine or reject, which programming languages, tools and phone hardware to use, and how to structure the project.
Anything I name in this prompt is an example, never an instruction. You may change my default assumptions (an Android app that encodes and decodes on the phone) if you say so and explain why. If you think part of my framing is wrong, say so and propose something better.
How to work
Today is November 2026, and your training data is out of date for this field. Search the live web and read primary sources: standards documents, papers with code, official repositories, vendor and Android documentation, and independent benchmarks. Run many searches (aim for 100+), read full documents, and use parallel research agents if your tool has them.
Then work in four steps:

1. Map the whole landscape of video compression: what ships, what is standardized, what is open source, research prototypes, paper-only ideas and pure theory. Explain how each important family works in plain words, how mature it is, and what it would cost on a phone. You choose the structure and the depth.
2. Decide and invent. Use your full knowledge and creativity to decide what to use as is, combine, invent or reject. Check whether each new idea already exists in papers or patents, and cite what you find.
3. Write the plan. Make it detailed enough that an engineering team that has never seen this conversation can start work the next day without asking questions.
4. Red-team the plan. List how it could fail and what you would do then.
What the plan must decide
You make every decision and justify it. For each major decision, list the real options, pick one, explain why, and say what evidence would change your mind. At minimum, decide:

- The overall approach for each mode, and what the two modes share.
- Programming languages, libraries and tools, for research experiments and for the later build.
- Which phone hardware to use (CPU instructions, GPU, NPU, hardware codecs) and for what.
- Whether and how to use machine learning: training data, model size and delivery to the phone.
- The file format and container design.
- How to measure success: metrics, test videos, devices, test methods, and how to score “best”.
- Team, schedule, cost and milestones, with a go/no-go test at each milestone.
- Legal, licensing and patent strategy, and how to ship and win adoption.
- How the later build and APK will be organized: modules, repositories and release path.
- What to research and prototype first, and the smallest experiment that proves the key numbers.
Quality rules
- Be honest and evidence-based. Date every fact. Never invent papers, authors, numbers, links or specifications. Write “unverified” when you cannot confirm something.
- Tag every claim: verified (primary source), verified (secondary source), author claim, estimate or speculation. Give every number its baseline, test set and hardware.
- Label every technology: shipping in products, standardized, open-source implementation, research prototype with code, paper only, or theory only.
- Be quantitative. Give estimates as ranges. Say plainly when something is unproven, hype or a bad idea.
- Do not ask me questions unless you are truly blocked. State your assumptions and continue.
Questions worth asking yourself
These are prompts for your thinking, not requirements. Ignore any that do not help.
- What does “lossless” really mean here: raw camera frames, bit-exact recompression of files that are already compressed, or near-lossless? The best answers may differ.
- If the decoder uses machine learning, how do you keep results identical on every phone chip, so files always decode correctly?
- What does each design cost in battery and heat, and when does the work run: during recording, or later while charging?
- Where does the phone’s own hardware help, and where does it get in the way?
Output format
Use this structure, or a better one you can justify:

1. Executive summary: the recommended approach, expected results with uncertainty, the top decisions and the first 30 days (one page).
2. Research findings: the landscape and what matters for our goal.
3. Inventions and design choices: expected gains, costs, risks, and the test that would prove or kill each one.
4. The full implementation plan: architecture, languages and tools, phases and milestones, testing and benchmarks, risks, legal, team and cost.
5. Appendices: sources with URLs and dates, claims list, glossary, open questions.
Do not write source code.   formulas or step-by-step descriptions or full logic flow or algorithms are fine where the plan needs them. Write in clear,  English , and all math and algorithms and logics full technical  and explain jargon the first time you use it. Use tables and simple text or Mermaid diagrams where they help.
If the answer is too long for one message write and save and send  in in an file .md if you preferred so you write as long as you want in an .md file and send one directly me ,  if it not possible do split the message into numbered parts an  d continue when I reply “continue       ,”. Never silently shorten or skip a section. End with a self-audit: your weakest claims, what you could not verify, and the 10 best next experiments.
