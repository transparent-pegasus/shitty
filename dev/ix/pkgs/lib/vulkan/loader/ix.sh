{% extends 't/ix.sh' %}

{% block use_data %}
lib/vulkan/loader/data
{% endblock %}

{% block lib_deps %}
{{super()}}
{% if linux %}
lib/xcb
{% endif %}
{% if darwin %}
lib/darwin/framework/CoreFoundation
{% endif %}
{% endblock %}

{% block bld_tool %}
{{super()}}
bld/reloc
bld/fake(tool_name=install_name_tool)
{% endblock %}

{% block cmake_flags %}
{% if linux %}
BUILD_WSI_XCB_SUPPORT=YES
{% else %}
BUILD_WSI_XCB_SUPPORT=NO
{% endif %}
BUILD_WSI_XLIB_SUPPORT=NO
{% endblock %}

{% block configure %}
{{super()}}
relocate "${VULKAN_LOADER_DATA}"
{% endblock %}

{% block install %}
{{super()}}
mkdir -p ${out}/include
>${out}/include/keep
{% endblock %}
