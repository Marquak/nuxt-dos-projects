# Putting this on GitHub

The contents of this folder can be the root of one repository, for example
`nuxt-dos-projects`. ORBIT87 contains both CPU builds. Each other program has
its own folder and README.

The repository is https://github.com/Marquak/nuxt-dos-projects.
The release ZIP is a copy of this same collection, useful for downloading
everything together.
The source tree also includes the small original-project DOS binaries so a
visitor can try them without setting up a compiler.

MIT is the prepared license for the project-authored material. Third-party
runtime terms are separate in THIRD_PARTY.md and OWLIC.TXT. Read those files
before publishing if you want to choose a different license for your work.

Only upload this prepared folder, not the surrounding working directory.
Dune II's generated executables, personal backups and DOS disk images belong
outside the repository. The .gitignore helps, but it is not a substitute for
checking the files being uploaded.

Before upload, check the snapshot:

```sh
python3 tools/verify_snapshot.py
```

After editing a source or document, SHA256SUMS will correctly report the old
snapshot no longer matches. Rebuild affected programs, run their relevant
tests and generate a new checksum list for the next release.
