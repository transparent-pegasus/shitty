{% extends '//die/c/autohell.sh' %}

{% block pkg_name %}
xcb-proto
{% endblock %}

{% block version %}
1.17.0
{% endblock %}

{% block fetch %}
https://www.x.org/releases/individual/proto/xcb-proto-{{self.version().strip()}}.tar.xz
2c1bacd2110f4799f74de6ebb714b94cf6f80fb112316b1219480fd22562148c
{% endblock %}

{% block bld_tool %}
bld/python
{% endblock %}

{% block bld_libs %}
lib/c
{% endblock %}

{% block postinstall %}
:
{% endblock %}
