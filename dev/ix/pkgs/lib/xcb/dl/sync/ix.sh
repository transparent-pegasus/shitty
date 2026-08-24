{% extends '//die/dl/lib.sh' %}

{% block lib_deps %}
lib/dlfcn
lib/xcb
{% endblock %}

{% block export_libs %}
libxcb-sync.a
{% endblock %}

{% block export_lib %}
xcb-sync
{% endblock %}
