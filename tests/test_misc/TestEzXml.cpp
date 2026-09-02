#include <TestEzXml.h>
#include <ezlibs/ezXml.hpp>
#include <ezlibs/ezCTest.hpp>

#include <iostream>
#include <string>

// Desactivation des warnings de conversion
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244)  // Conversion from 'double' to 'float', possible loss of data
#pragma warning(disable : 4305)  // Truncation from 'double' to 'float'
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzXml_ParsingOK() {
    const auto &doc =
            u8R"(
 < config > 
	<!-- Comment 1 -->
    <NumberOneLine>60</NumberOneLine>
    <Tests> 
	    <!-- Comment 2 -->
        <Test name="test1" number="50"/>
        <Test name ="test2" number="100"/>
        <Test name= "test3" number="150"/>
        <Test name = "test4" number="200">
            <SubTest name="subTest1" number="250"/>
            <SubTest name="subTest2" number="300"/>
            <bool>false</bool>
            <bool>true</bool>
        </Test>
        <Test/>
    </Tests>
</config>
)";
    ez::Xml xml;
    if (!xml.parseString(doc))
        return false;
    std::cout << xml.dump() << std::endl;
    if (xml.getRoot().getChildren().empty())
        return false;
    const auto &rootChildrens = xml.getRoot().getChildren();
    if (rootChildrens.size() != 1U)
        return false;
    if (rootChildrens[0].getName() != "config")
        return false;
    const auto &configChildrens = rootChildrens[0].getChildren();
    if (configChildrens.size() != 3U)
        return false;
    if (configChildrens[0].getContent() != "<!-- Comment 1 -->")
        return false;
    if (configChildrens[1].getName() != "NumberOneLine")
        return false;
    if (configChildrens[1].getContent() != "60")
        return false;
    if (configChildrens[2].getName() != "Tests")
        return false;
    const auto &testsChildrens = configChildrens[2].getChildren();
    if (testsChildrens.size() != 6U)
        return false;
    if (testsChildrens[0].getContent() != "<!-- Comment 2 -->")
        return false;
    if (testsChildrens[1].getName() != "Test")
        return false;
    if (testsChildrens[1].getParentNodeName() != "Tests")
        return false;
    if (!testsChildrens[1].isAttributeExist("name"))
        return false;
    if (testsChildrens[1].getAttribute("name") != "test1")
        return false;
    if (testsChildrens[1].getAttribute("number") != "50")
        return false;
    if (testsChildrens[2].getAttribute("name") != "test2")
        return false;
    if (testsChildrens[2].getAttribute("number") != "100")
        return false;
    if (testsChildrens[3].getAttribute("name") != "test3")
        return false;
    if (testsChildrens[3].getAttribute("number") != "150")
        return false;
    if (testsChildrens[4].getAttribute("name") != "test4")
        return false;
    if (testsChildrens[4].getAttribute("number") != "200")
        return false;
    if (testsChildrens[5].getName() != "Test")
        return false;
    const auto &test4Childrens = testsChildrens[4].getChildren();
    if (test4Childrens.size() != 4U)
        return false;
    if (test4Childrens[0].getName() != "SubTest")
        return false;
    if (test4Childrens[0].getAttribute("name") != "subTest1")
        return false;
    if (test4Childrens[0].getAttribute("number") != "250")
        return false;
    if (test4Childrens[1].getName() != "SubTest")
        return false;
    if (test4Childrens[1].getAttribute("name") != "subTest2")
        return false;
    if (test4Childrens[1].getAttribute<int32_t>("number") != 300)
        return false;
    if (test4Childrens[2].getName() != "bool")
        return false;
    if (test4Childrens[2].getContent<bool>() != false)
        return false;
    if (test4Childrens[3].getName() != "bool")
        return false;
    if (test4Childrens[3].getContent<bool>() != true)
        return false;
    return true;
}

// all attributes value must be some strings
bool TestEzXml_ParsingNOK_0() {
    const auto &doc =
            u8R"(
<config>
    <Test name="test1" number=5/>
</config>
)";
    ez::Xml xml;
    if (xml.parseString(doc))
        return false;
    std::cout << xml.dump() << std::endl;
    return true;
}

// to tag end
bool TestEzXml_ParsingNOK_1() {
    const auto &doc =
            u8R"(
<config>
<config>
)";
    ez::Xml xml;
    if (!xml.parseString(doc))
        return false;
    std::cout << xml.dump() << std::endl;
    return true;
}

bool TestEzXml_Writing_1() {
    ez::Xml xml("test");
    auto &rootNode = xml.getRoot();
    rootNode.setName("config");
    rootNode.addComment("Comment 1");
    rootNode.addChild("NumberOneLine").setContent(60);
    auto &testsNode = rootNode.addChild("Tests");
    testsNode.addComment("Comment 2");
    testsNode.addChild("Test").addAttribute("name", "test1").addAttribute("number") << 50;
    testsNode.addChild("Test").addAttribute("name", "test2").addAttribute("number") << 100;
    testsNode.addChild("Test").addAttribute("name", "test3").addAttribute("number") << 150;
    auto &subNode = testsNode.addChild("Test");
    subNode.addAttribute("name", "test4").addAttribute("number") << 200;
    subNode.addChild("SubTest").addAttribute("name", "subTest1").addAttribute("number") << 250;
    subNode.addChild("SubTest").addAttribute("name", "subTest2").addAttribute("number") << 300;

    const auto result = xml.dump();
    const auto expected = u8R"(<config>
  <!-- Comment 1 -->
  <NumberOneLine>60</NumberOneLine>
  <Tests>
    <!-- Comment 2 -->
    <Test name="test1" number="50"/>
    <Test name="test2" number="100"/>
    <Test name="test3" number="150"/>
    <Test name="test4" number="200">
      <SubTest name="subTest1" number="250"/>
      <SubTest name="subTest2" number="300"/>
    </Test>
  </Tests>
</config>
)";

    CTEST_ASSERT(result == expected);
    return true;
}

bool TestEzXml_EscapeUnescapeXml() {
    std::string input = "<tag>Value&\"'</tag>";
    std::string escaped = ez::xml::Node::escapeXml(input);
    CTEST_ASSERT(escaped.find("&lt;") != std::string::npos);
    CTEST_ASSERT(escaped.find("&gt;") != std::string::npos);
    CTEST_ASSERT(escaped.find("&amp;") != std::string::npos);
    CTEST_ASSERT(escaped.find("&quot;") != std::string::npos);
    CTEST_ASSERT(escaped.find("&apos;") != std::string::npos);

    std::string unescaped = ez::xml::Node::unEscapeXml(escaped);
    CTEST_ASSERT(unescaped == input);
    return true;
}

// a literal escape sequence written by the user must not decay : "&quot;"
// escapes to "&amp;quot;" and must come back as "&quot;", not as a real
// quote. this holds only if "&amp;" is the LAST pattern unescaped
bool TestEzXml_EscapeSequenceIsNotDecoded() {
    const std::string input = "&quot; &amp; &lt; plain &";
    const std::string escaped = ez::xml::Node::escapeXml(input);
    CTEST_ASSERT(ez::xml::Node::unEscapeXml(escaped) == input);
    return true;
}

// a node stores RAW text : dump escapes once, the parser unescapes once.
// escaping at set time made a parsed document escape a second time, and a
// stored "&" came back as "&amp;" after a dump/parse round trip
bool TestEzXml_RoundTripSpecialChars() {
    const std::string content = "a < b & c > d \"quoted\" 'single'";
    const std::string attrValue = "x=\"1\" & y<2";

    ez::Xml writer("config");
    ez::xml::Node &writerRoot = writer.getRoot();
    writerRoot.addChild("line").setContent(content);
    writerRoot.addChild("item").addAttribute("value", attrValue);
    const std::string doc = writer.dump();

    // the escaped document carries no raw markup character inside the text
    // nor inside the quoted attribute : this is what keeps it parseable
    CTEST_ASSERT(doc.find("&lt;") != std::string::npos);
    CTEST_ASSERT(doc.find("&amp;") != std::string::npos);

    ez::Xml reader;
    CTEST_ASSERT(reader.parseString(doc));
    const ez::xml::Nodes &roots = reader.getRoot().getChildren();
    CTEST_ASSERT(roots.size() == 1U);
    const ez::xml::Nodes &children = roots[0].getChildren();
    CTEST_ASSERT(children.size() == 2U);
    CTEST_ASSERT(children[0].getName() == "line");
    CTEST_ASSERT(children[0].getContent() == content);
    CTEST_ASSERT(children[1].getName() == "item");
    CTEST_ASSERT(children[1].getAttribute("value") == attrValue);

    // stable through a second cycle : re-dumping the parsed node gives back
    // the very same document (the parsed tree carries a "root" wrapper, so
    // the comparison starts at the config node the writer dumped)
    CTEST_ASSERT(roots[0].dump() == doc);
    return true;
}

// a generic walk of what a node carries : a converter to another format
// cannot ask key by key, it has to enumerate
bool TestEzXml_GetAttributes() {
    ez::xml::Node node("test");
    CTEST_ASSERT(node.getAttributes().empty());
    node.addAttribute("beta", "2");
    node.addAttribute("alpha", "1");
    const ez::xml::Node::Attributes &attributes = node.getAttributes();
    CTEST_ASSERT(attributes.size() == 2U);
    // a std::map walk is key-ordered, whatever the insertion order
    ez::xml::Node::Attributes::const_iterator it = attributes.begin();
    CTEST_ASSERT(it->first == "alpha");
    CTEST_ASSERT(it->second.getValue() == "1");
    ++it;
    CTEST_ASSERT(it->first == "beta");
    CTEST_ASSERT(it->second.getValue() == "2");
    return true;
}

bool TestEzXml_NodeOperations() {
    ez::xml::Node node("testNode");
    node.setContent("TestContent");
    CTEST_ASSERT(node.getName() == "testNode");
    CTEST_ASSERT(node.getContent() == "TestContent");

    node.addAttribute("attr1", "value1");
    CTEST_ASSERT(node.isAttributeExist("attr1"));
    CTEST_ASSERT(node.getAttribute("attr1") == "value1");
    CTEST_ASSERT(!node.isAttributeExist("nonexistent"));

    return true;
}

bool TestEzXml_GetOrAddChild() {
    ez::xml::Node parent("parent");
    auto& child1 = parent.addChild("child");
    auto& child2 = parent.getOrAddChild("child");

    // Should return the existing child
    CTEST_ASSERT(&child1 == &child2);

    // Should create a new child
    auto& newChild = parent.getOrAddChild("newChild");
    CTEST_ASSERT(newChild.getName() == "newChild");

    return true;
}

bool TestEzXml_GetChildNull() {
    ez::xml::Node parent("parent");
    parent.addChild("child1");

    auto* found = parent.getChild("child1");
    CTEST_ASSERT(found != nullptr);

    auto* notFound = parent.getChild("nonexistent");
    CTEST_ASSERT(notFound == nullptr);

    return true;
}

bool TestEzXml_AddChilds() {
    ez::xml::Node parent("parent");
    ez::xml::Nodes children;
    children.push_back(ez::xml::Node("child1"));
    children.push_back(ez::xml::Node("child2"));
    children.push_back(ez::xml::Node("child3"));

    parent.addChilds(children);
    CTEST_ASSERT(parent.getChildren().size() == 3);

    return true;
}

bool TestEzXml_AttributeWithTemplateTypes() {
    ez::xml::Node node("test");
    node.addAttribute("intAttr", 42);
    node.addAttribute("floatAttr", 3.14f);
    node.addAttribute("doubleAttr", 2.718);

    CTEST_ASSERT(node.getAttribute<int>("intAttr") == 42);
    float f = node.getAttribute<float>("floatAttr");
    CTEST_ASSERT(f > 3.13f && f < 3.15f);

    return true;
}

// the law of a REAL document (an svg from inkscape, illustrator, an icon
// pack) : the xml prologue, a doctype and a comment before the root are
// not elements — the root element is found among the top level nodes,
// whole, with its attributes and its children
bool TestEzXml_ParsesAPrologueAndADoctype() {
    const std::string doc =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n"
        "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n"
        "<!-- made by hand -->\n"
        "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\">\n"
        "  <path d=\"M0 0h24v24H0z\"/>\n"
        "</svg>\n";
    ez::Xml xml;
    CTEST_ASSERT(xml.parseString(doc));
    const ez::xml::Node* pSvg = nullptr;
    for (const auto& child : xml.getRoot().getChildren()) {
        if (child.getName() == "svg") {
            pSvg = &child;
        }
    }
    CTEST_ASSERT(pSvg != nullptr);
    CTEST_ASSERT(pSvg->getAttribute<std::string>("viewBox") == "0 0 24 24");
    CTEST_ASSERT(pSvg->getChildren().size() == 1U);
    CTEST_ASSERT(pSvg->getChildren()[0].getName() == "path");
    CTEST_ASSERT(pSvg->getChildren()[0].getAttribute<std::string>("d") == "M0 0h24v24H0z");
    return true;
}

// the law of an EDITOR document (inkscape, illustrator) : the attributes
// spread over lines and tabs, one per line, a self-closing tag written
// with a space before its slash — every attribute lands whole, every
// child is found, the nesting holds
bool TestEzXml_ParsesAttributesSpreadOverLines() {
    const std::string doc =
        "<svg\n"
        "   xmlns:dc=\"http://purl.org/dc/elements/1.1/\"\n"
        "   width=\"9.9250002mm\"\n"
        "\tviewBox=\"0 0 9.9250002 8.3374996\"\n"
        "   id=\"svg8\">\n"
        "  <defs\n"
        "     id=\"defs2\" />\n"
        "  <g\n"
        "     id=\"layer1\"\n"
        "     transform=\"translate(-53.245835,-252.47712)\">\n"
        "    <path\n"
        "       style=\"fill:#000000;fill-opacity:1\"\n"
        "       d=\"m 54,253 c 1,2 3,4 5,6 z\"\n"
        "       id=\"path815\" />\n"
        "  </g>\n"
        "</svg>\n";
    ez::Xml xml;
    CTEST_ASSERT(xml.parseString(doc));
    const ez::xml::Nodes &roots = xml.getRoot().getChildren();
    CTEST_ASSERT(roots.size() == 1U);
    const ez::xml::Node &svg = roots[0];
    CTEST_ASSERT(svg.getName() == "svg");
    CTEST_ASSERT(svg.getAttribute<std::string>("width") == "9.9250002mm");
    CTEST_ASSERT(svg.getAttribute<std::string>("viewBox") == "0 0 9.9250002 8.3374996");
    CTEST_ASSERT(svg.getAttribute<std::string>("id") == "svg8");
    CTEST_ASSERT(svg.getChildren().size() == 2U);
    CTEST_ASSERT(svg.getChildren()[0].getName() == "defs");
    CTEST_ASSERT(svg.getChildren()[0].getAttribute<std::string>("id") == "defs2");
    const ez::xml::Node &group = svg.getChildren()[1];
    CTEST_ASSERT(group.getName() == "g");
    CTEST_ASSERT(group.getAttribute<std::string>("transform") == "translate(-53.245835,-252.47712)");
    CTEST_ASSERT(group.getChildren().size() == 1U);
    const ez::xml::Node &path = group.getChildren()[0];
    CTEST_ASSERT(path.getName() == "path");
    CTEST_ASSERT(path.getAttribute<std::string>("d") == "m 54,253 c 1,2 3,4 5,6 z");
    CTEST_ASSERT(path.getAttribute<std::string>("style") == "fill:#000000;fill-opacity:1");
    return true;
}

bool TestEzXml_ReplaceAll() {
    std::string str = "hello world hello";
    ez::xml::Node::replaceAll(str, "hello", "hi");
    CTEST_ASSERT(str == "hi world hi");

    // Test with empty string
    std::string str2 = "test";
    ez::xml::Node::replaceAll(str2, "", "x");
    CTEST_ASSERT(str2 == "test");

    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

#define IfTestExist(v)            \
    if (vTest == std::string(#v)) \
    return v()

bool TestEzXml(const std::string &vTest) {
    IfTestExist(TestEzXml_ParsingOK);
    else IfTestExist(TestEzXml_ParsingNOK_0);
    else IfTestExist(TestEzXml_ParsingNOK_1);
    else IfTestExist(TestEzXml_Writing_1);
    else IfTestExist(TestEzXml_EscapeUnescapeXml);
    else IfTestExist(TestEzXml_EscapeSequenceIsNotDecoded);
    else IfTestExist(TestEzXml_RoundTripSpecialChars);
    else IfTestExist(TestEzXml_GetAttributes);
    else IfTestExist(TestEzXml_NodeOperations);
    else IfTestExist(TestEzXml_GetOrAddChild);
    else IfTestExist(TestEzXml_GetChildNull);
    else IfTestExist(TestEzXml_AddChilds);
    else IfTestExist(TestEzXml_AttributeWithTemplateTypes);
    else IfTestExist(TestEzXml_ReplaceAll);
    else IfTestExist(TestEzXml_ParsesAPrologueAndADoctype);
    else IfTestExist(TestEzXml_ParsesAttributesSpreadOverLines);
    return false;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
#pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
