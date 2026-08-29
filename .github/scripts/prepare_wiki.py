#!/usr/bin/env python3
"""Flatten docs/ into a GitHub wiki tree (Home.md, sidebar, rewritten links)."""

from __future__ import annotations

import argparse
import re
import shutil
from pathlib import Path

LINK_RE = re.compile(r"\[([^\]]+)\]\(([^)]+)\)")

GUIDES = (
    ("getting-started.md", "Getting started"),
    ("architecture.md", "Architecture"),
    ("api-overview.md", "API overview"),
)


def page_title(path: Path) -> str:
    for line in path.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if stripped.startswith("# "):
            return stripped[2:].strip()
    return path.stem.replace("-", " ")


def wiki_name(path: Path) -> str:
    return Path(path).stem


def rewrite_href(href: str, source: Path, docs_root: Path, repo: str, sha: str) -> str:
    if href.startswith(("http://", "https://", "mailto:", "#")):
        return href

    path_part, _, fragment = href.partition("#")
    if not path_part:
        return href

    resolved = (source.parent / path_part).resolve()
    try:
        relative = resolved.relative_to(docs_root.resolve())
    except ValueError:
        try:
            repo_relative = resolved.relative_to(docs_root.parent.resolve())
        except ValueError:
            return href
        url = f"https://github.com/{repo}/blob/{sha}/{repo_relative.as_posix()}"
        return url if not fragment else f"{url}#{fragment}"

    if relative.suffix.lower() != ".md":
        url = f"https://github.com/{repo}/blob/{sha}/docs/{relative.as_posix()}"
        return url if not fragment else f"{url}#{fragment}"

    page = wiki_name(relative)
    if relative.name.lower() == "readme.md":
        page = "Home"
    return f"{page}#{fragment}" if fragment else page


def rewrite_markdown(text: str, source: Path, docs_root: Path, repo: str, sha: str) -> str:
    def replace(match: re.Match[str]) -> str:
        label, href = match.group(1), match.group(2)
        return f"[{label}]({rewrite_href(href, source, docs_root, repo, sha)})"

    return LINK_RE.sub(replace, text)


def collect_pages(docs_root: Path) -> list[Path]:
    pages = [docs_root / "README.md"]
    for name, _title in GUIDES:
        candidate = docs_root / name
        if candidate.is_file():
            pages.append(candidate)
    tutorials = sorted((docs_root / "tutorials").glob("*.md"))
    pages.extend(tutorials)
    return [path for path in pages if path.is_file()]


def write_sidebar(out_dir: Path, tutorials: list[Path]) -> None:
    lines = [
        "* [Home](Home)",
        "* Guides",
    ]
    for name, title in GUIDES:
        if (out_dir / name).is_file():
            lines.append(f"  * [{title}]({Path(name).stem})")
    if tutorials:
        lines.append("* Tutorials")
        for path in tutorials:
            lines.append(f"  * [{page_title(path)}]({path.stem})")
    (out_dir / "_Sidebar.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_footer(out_dir: Path) -> None:
    (out_dir / "_Footer.md").write_text(
        "AP2 — Application Primitives. MIT licensed. "
        "Copyright (c) 2024-2026 Jack Waechter. "
        "Generated from `docs/` in the repository.\n",
        encoding="utf-8",
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--docs", type=Path, default=Path("docs"))
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--repo", default="youthx/AP2")
    parser.add_argument("--sha", default="HEAD")
    args = parser.parse_args()

    docs_root = args.docs.resolve()
    out_dir = args.out.resolve()
    if out_dir.exists():
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True)

    tutorials: list[Path] = []
    for source in collect_pages(docs_root):
        text = source.read_text(encoding="utf-8")
        rewritten = rewrite_markdown(text, source, docs_root, args.repo, args.sha)
        dest_name = "Home.md" if source.name.lower() == "readme.md" else f"{source.stem}.md"
        (out_dir / dest_name).write_text(rewritten, encoding="utf-8")
        if source.parent.name == "tutorials":
            tutorials.append(source)

    write_sidebar(out_dir, tutorials)
    write_footer(out_dir)


if __name__ == "__main__":
    main()
