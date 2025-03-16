<?xml version="1.0" encoding="UTF-8" ?>
<xsl:stylesheet version="1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
    <xsl:output method="text" omit-xml-declaration="no" indent="no"/>

<xsl:template match="/unit"><xsl:variable name="name" select="substring(@name,5,string-length(@name)-8)" />---
title: Unit <xsl:value-of select="$name" />
permalink: /docs/<xsl:value-of select="$name" />/
---

# Description

<xsl:value-of select="description/detailed" />

# Overview
<xsl:if test="count(function[@type='function']) &gt; 0" >
## Functions

| Name | Declaration |
| ------ | ------ |
<xsl:for-each select="function[@type='function']">| [<xsl:value-of select="@name"/>](#<xsl:value-of select="@name"/>) | <xsl:value-of select="@declaration" /> |
</xsl:for-each>
</xsl:if>

<xsl:if test="count(function[@type='procedure']) &gt; 0" >
## Procedures

| Name | Declaration |
| ------ | ------ |
<xsl:for-each select="function[@type='procedure']">| [<xsl:value-of select="@name"/>](#<xsl:value-of select="@name"/>) | <xsl:value-of select="@declaration" /> |
</xsl:for-each>
</xsl:if>
<xsl:if test="count(type) &gt; 0" >
## Types

| Name | Description |
| ------ | ------ |
<xsl:for-each select="type">| <xsl:value-of select="@name"/> | <xsl:value-of select="description/detailed" /> |
</xsl:for-each>
</xsl:if>
## Structures and Classes
<xsl:if test="count(structure) &gt; 0" >
| Name | Description |
| ------ | ------ |
<xsl:for-each select="structure">| **<xsl:value-of select="@type"/>** [<xsl:value-of select="@name"/>](#<xsl:value-of select="@name"/>) | <xsl:value-of select="description/detailed" /> |
</xsl:for-each>
</xsl:if>
# Functions
<xsl:for-each select="function[@type='function']">
## <xsl:value-of select="@name"/>
### Description
<xsl:value-of select="description/detailed" />

<xsl:if test="count(param) &gt; 0" >
### Params

| Name | Description |
| ------ | ------ |
<xsl:for-each select="param">| <xsl:value-of select="@name" /> | <xsl:value-of select="text()" /> |
</xsl:for-each>
</xsl:if>

</xsl:for-each>

# Procedures
<xsl:for-each select="function[@type='procedure']">
## <xsl:value-of select="@name"/>
### Description
<xsl:value-of select="description/detailed" />
<xsl:if test="count(param) &gt; 0" >
### Params

| Name | Description |
| ------ | ------ |
<xsl:for-each select="param">| <xsl:value-of select="@name" /> | <xsl:value-of select="text()" /> |
</xsl:for-each>
</xsl:if>

</xsl:for-each>
</xsl:template>
</xsl:stylesheet>