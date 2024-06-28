Frequently Asked Questions
==========================

.. contents:: Table of Contents

**Q:** Is there a list of supported models?
-------------------------------------------

**A:** No, Haier has a wide range of models, and different models are available in different regions. Sometimes the same model can have different names depending on the country where it is sold. The only way to know if your model is supported is to try it out.

**Q:** How can I be sure that your component will support the selected model when buying a new AC?
------------------------------------------------------------------------------------------------

**A:** Unfortunately, there is no guarantee until you try it. The best way is to check the user manual. Usually, if the AC supports ESP modules, there will be information about which application the native module works with. So far, all models that work with smartAir2, hOn, and EVO applications also work with this component, although sometimes additional tuning may be required.

**Q:** The component is not working, how can I troubleshoot it?
---------------------------------------------------------------

**A:** You can start by checking the logs. If you see any errors, you can try to fix them. If you are unable to fix them, you can ask for help on the ESPHome Discord server.

**Q:** I am constantly seeing the warning "Component haier took a long time for an operation" in the logs, is this normal?
----------------------------------------------------------------------------------------------------------------------

**A:** Yes, this message has always been there, but in the latest versions, it was changed to a warning. This message is shown when the component is too busy processing something. The biggest delays are usually related to logging and operations with the web server. If you want to reduce the chance of seeing this message, you can decrease the log level to "warning". The warning level is sufficient for a fully working system. This message is usually not a problem but can indicate that some other component (like Wi-Fi) that should work in real-time is suffering from delays. For more information, refer to: https://github.com/esphome/issues/issues/4717

**Q:** I am seeing the warning "Answer handler error, msg=01, answ=03, err=7". What should I do?
--------------------------------------------------------------------------------------------

**A:** This warning means that the AC denied the control command. It can happen in two cases: either the AC is using a different type of control or the structure of the status packet is different. You can try using the `control_method: SET_SINGLE_PARAMETER`. If that doesn't help, you can try to figure out the size of different parts of the status packet using this method: `Haier protocol overview <./docs/protocol_overview.rst>`. If nothing helps, you can create an issue on GitHub.

**Q:** My ESP is communicating with the AC, but I can't control it. Or I can control it, but my sensors show the wrong information.
-------------------------------------------------------------------------------------------------------------------------------

**A:** Most likely, you have one of two problems: either the wrong control method or the wrong status packet structure. You can try using the `control_method: SET_SINGLE_PARAMETER`. If that doesn't help, you can try to figure out the size of different parts of the status packet using this method: `Haier protocol overview <./docs/protocol_overview.rst>`.

**Q:** My AC has a cool feature that is not supported by your component. Can you add it?
----------------------------------------------------------------------------------------

**A:** First, you need to figure out if the feature is supported by the serial protocol. There is some functionality that is supported only by the IR remote. The easiest way to check is by using the IR remote:

- Start capturing logs from your ESP modules.
- Wait 10 - 15 seconds.
- Enable the feature using the remote.
- Wait 10 - 15 seconds.
- Disable the feature using the remote.
- Wait 10 - 15 seconds.
- Stop capturing logs.
- Check the logs for changes in the status packet.

If all messages that look like this "Frame found: type 02, data: 6D 01 ..." are the same, the feature you want to add is not supported by the serial protocol. If you see some changes in the status packet, you can create a feature request on GitHub with the logs you collected.
