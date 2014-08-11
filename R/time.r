## git2r, R bindings to the libgit2 library.
## Copyright (C) 2013-2014 The git2r contributors
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License, version 2,
## as published by the Free Software Foundation.
##
## git2r is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License along
## with this program; if not, write to the Free Software Foundation, Inc.,
## 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

setAs(from="git_time",
      to="character",
      def=function(from)
      {
          as.character(as(from, "POSIXct"))
      }
)

setAs(from="git_time",
      to="POSIXct",
      def=function(from)
      {
          as.POSIXct(from@time + from@offset*60,
                     origin="1970-01-01",
                     tz="GMT")
      }
)

##' Brief summary of branch
##'
##' @aliases show,git_time-methods
##' @docType methods
##' @param object The time \code{object}
##' @return None (invisible 'NULL').
##' @keywords methods
##' @export
setMethod("show",
          signature(object = "git_time"),
          function(object)
          {
              cat(sprintf("%s\n", as(object, "character")))
          }
)
