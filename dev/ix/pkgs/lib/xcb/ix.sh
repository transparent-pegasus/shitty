{% extends '//die/c/autohell.sh' %}

{% block pkg_name %}
libxcb
{% endblock %}

{% block version %}
1.17.0
{% endblock %}

{% block fetch %}
https://www.x.org/releases/individual/lib/libxcb-{{self.version().strip()}}.tar.xz
599ebf9996710fea71622e6e184f3a8ad5b43d0e5fa8c4e407123c88a59a6d55
{% endblock %}

{% block bld_tool %}
bld/python
{% endblock %}

{% block lib_deps %}
lib/c
aux/xcb/proto
lib/xau
{% endblock %}

{% block configure %}
# Do not let all_system=1 leak optional host libraries into this target build.
# Declared IX dependencies remain visible through PKG_CONFIG_PATH.
export PKG_CONFIG_LIBDIR=/nowhere
{{super()}}
{% endblock %}
