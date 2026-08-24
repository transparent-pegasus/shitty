{% extends '//die/dl/lib.sh' %}

{% block lib_deps %}
lib/dlfcn
lib/xcb
{% endblock %}

{% block export_libs %}
libxcb-dri3.a
{% endblock %}

{% block export_lib %}
xcb-dri3
{% endblock %}
