This template contains the following files:

outlineAndPlan.tex : The main file.
config.tex         : Enter your name, the provisional project title etc., near the start of this file.
mainText.tex       : All of the main text is included in this file.
logo_black.png     : The University of Leeds logo. You should not alter this.
refs.bib           : The bibliography as a bibtex file, if you wish to cite references.

There are various ways to edit LaTeX files:
- Online with Overleaf (free subscription for individual accounts at last time of checking).
- Using a dedicated package such as texmaker, texmanager, texshop etc.
- Use you favourite text editor (many source code editors understand LaTeX syntax).

In the latter case you would normally convert the .tex files to PDF from the command line:
> pdflatex outlineAndPlan
> bibtex outlineAndPlan
> pdflatex outlineAndPlan
> pdflatex outlineAndPlan
This will produce the file 'outlineAndPlan.pdf' with references inserted. If you are not using the .bib bibliograpy file,
you can omit the first two lines. If you do not use any referencing at all, you only need to call 'pdflatex' once.

If you do not have 'pdflatex' installed on your system, you can use 'latex' instead, which produces a .dvi file.
This can then be converted to PDF using dvipdf.

If using a school machine and you get errors about a style file not being recognised, try first loading the most recent
version of texlive (i.e. type 'module avail' to see which is the most recent, and then 'module load texlive/20..').

DAH/13/9/2021
SW/12/3/2015

