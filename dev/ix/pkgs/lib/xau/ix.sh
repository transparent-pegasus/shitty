{% extends '//die/c/autohell.sh' %}

{% block pkg_name %}
libXau
{% endblock %}

{% block version %}
1.0.12
{% endblock %}

{% block fetch %}
https://www.x.org/releases/individual/lib/libXau-{{self.version().strip()}}.tar.xz
74d0e4dfa3d39ad8939e99bda37f5967aba528211076828464d2777d477fc0fb
{% endblock %}

{% block lib_deps %}
lib/c
aux/x11/proto
{% endblock %}
