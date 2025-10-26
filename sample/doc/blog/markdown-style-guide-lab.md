---
title: "Lab Report Markdown Style Guide"
description: "House rules for writing chemistry notes in Markdown"
pubDate: "2024-04-23"
heroImage: "../../assets/blog-placeholder-2.jpg"
status: publish
tags:
  - style
  - documentation
category: guide
---

Chemistry folk adore structure, so our Markdown guide does not leave much to chance. Here are the conventions we follow when logging bench work or drafting posts like this one.

## Headings keep reactions clear

Use an `h2` heading for each major stage of a synthesis. Title it with the reagent or outcome: `## Oxidation to aldehyde`. Sub-steps, such as quench or extraction, earn an `h3`. This helps readers skim the outline before diving into stoichiometry.

## Tables list compositions

When listing charge masses or solvent ratios, build a table. It travels well between static sites and exported PDFs. For example:

| Component | Amount | Notes |
| --- | --- | --- |
| Sodium acetate | 6.5 g | Dried overnight |
| Acetic acid | 4.0 g | Glacial |
| Water | 40 mL | Deionised |

## Emphasise observations, not feelings

We write "solution turned pale green" rather than "looked lovely". A non-rhotic senior once said, "describe the flask, not your soul." Adopting that tone keeps the archive useful.

## Code blocks for automation

When you include a snippet from an instrument or a small Python script, fence it with three backticks and add the language tag. That way syntax highlighting survives in the Astro build.

```
python
for step in protocol:
    print(step.name)
```

## Images earn captions

If you embed a diagram of glassware, add a caption beneath with exposure details or the lot number. It proves you paid attention and helps auditors retrace your work.

Follow these habits and your notes will stay consistent long after the reagents change.
