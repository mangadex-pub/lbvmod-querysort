vmod_querysort
==============

Varnish Module (vmod) for sorting query parameters. The code is a fork of
`std.querysort`_ with the following modifications:

1. Query parameters are sorted by key only.
2. The sort is stable: the relative ordering of duplicate parameters is preserved.
3. PHP array parameter keys (e.g. `foo[]=1&foo[1]=2`) are handled correctly
   (the keys compare as equal), provided the square brackets are not percent-encoded.

.. _std.querysort: https://varnish-cache.org/docs/trunk/reference/vmod_std.html#std-querysort

