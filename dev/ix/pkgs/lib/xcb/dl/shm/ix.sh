{% extends '//die/dl/lib.sh' %}

{% block lib_deps %}
lib/dlfcn
lib/xcb
{% endblock %}

{% block export_libs %}
libxcb-shm.a
{% endblock %}

{% block export_lib %}
xcb-shm
{% endblock %}
