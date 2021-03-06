# vim: set et ft=gnuplot sw=4 :

set terminal tikz standalone color size 7.5cm,6cm font '\scriptsize' preamble '\usepackage{times,microtype}'
set output "gen-graph-scatter-dds.tex"

load "parula.pal"
load "common.gnuplot"

set xlabel "DFS Search Time (ms)"
set ylabel "DDS Search Time (ms)" offset 0.5
set logscale x
set logscale y
set format x '$10^{%T}$'
set format y '$10^{%T}$'
set key horiz rmargin maxcols 1 samplen 1 width -3
set size square

plotfile="searchtimes.data"
satcol="sat"
famcol="family"
xcol=norestarts
ycol=dds

load "scatter.gnuplot"

