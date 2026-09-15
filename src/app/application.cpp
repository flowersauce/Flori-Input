/**
 * @file application.cpp
 * @brief 组合应用服务、Slint 界面和 Windows 平台集成。
 */
#include "app/application.h"
#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <memory>
#include <optional>
#include <slint.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <windows.h>
#include "app/app_config.h"
#include "app/clicker_runtime.h"
#include "app/ui_resources.h"
#include "core/input_router.h"
#include "core/key_catalog.h"
#include "core/task_scheduler.h"
#include "main.h"
#include "platform/event_script_file.h"
#include "platform/paste_input.h"
#include "platform/windows_input_backend.h"
#include "platform/windows_platform.h"
#include "platform/windows_trigger_executor.h"

namespace flori_input
{
	namespace
	{
		using Capture = ClickerController::Capture;
		/** @brief 按固定小数位和独立区域设置格式化数值。 */
		std::string fixed(const double number, const int precision)
		{
			std::ostringstream stream;
			stream.imbue(std::locale::classic());
			stream << std::fixed << std::setprecision(precision) << number;
			return stream.str();
		}
		/** @brief 查询指定语言的热键显示名称。 */
		std::string keyName(const std::uint32_t key, const bool chinese, const UiResources &resources)
		{
			if (!key)
			{
				return resources.text("common.custom", chinese);
			}
			const auto found = keyMap.find(key);
			if (found == keyMap.end())
			{
				return resources.text("common.unknown", chinese);
			}
			return resources.keyLabel(found->second, chinese);
		}
		/** @brief 本地化触发器控制器状态。 */
		std::string status(const ClickerController &controller, const bool chinese, const UiResources &resources)
		{
			return resources.message(controller.statusText(), chinese);
		}
		/** @brief 获取指定虚拟桌面坐标所在显示器的物理边界。 */
		RECT monitorAtPoint(const POINT point)
		{
			MONITORINFO monitor{.cbSize = sizeof(MONITORINFO)};
			if (const auto handle = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST); handle && GetMonitorInfoW(handle, &monitor))
			{
				return monitor.rcMonitor;
			}
			const int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
			const int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
			return {.left = x, .top = y, .right = x + GetSystemMetrics(SM_CXVIRTUALSCREEN), .bottom = y + GetSystemMetrics(SM_CYVIRTUALSCREEN)};
		}
		/** @brief 记录单窗口的位置和信息卡片相对鼠标的朝向。 */
		struct CoordinatePopupPlacement
		{
			POINT position;
			bool  infoOnRight;
			bool  infoOnBottom;
		};
		/** @brief 将鼠标置于小窗口边缘，并使信息卡片避开所在显示器边界。 */
		CoordinatePopupPlacement coordinatePopupPlacement(const POINT cursor, const RECT monitor, const slint::PhysicalSize size)
		{
			const std::int64_t anchorX		= size.width - size.width / 12;
			const std::int64_t anchorY		= size.height - size.height / 7;
			const bool		   infoOnRight	= static_cast<std::int64_t>(cursor.x) - monitor.left < anchorX;
			const bool		   infoOnBottom = static_cast<std::int64_t>(cursor.y) - monitor.top < anchorY;
			const auto		   place		= [](const std::int64_t pointer,
									 const std::int64_t anchor,
									 const bool			flipped,
									 const std::int64_t begin,
									 const std::int64_t end,
									 const std::int64_t extent)
			{
				const std::int64_t desired = flipped ? pointer : pointer - anchor;
				return static_cast<LONG>(std::clamp(desired, begin, std::max(begin, end - extent)));
			};
			return {.position	  = {.x = place(cursor.x, anchorX, infoOnRight, monitor.left, monitor.right, size.width),
									 .y = place(cursor.y, anchorY, infoOnBottom, monitor.top, monitor.bottom, size.height)},
					.infoOnRight  = infoOnRight,
					.infoOnBottom = infoOnBottom};
		}
		/** @brief 让坐标信息窗保持置顶但不进入任务栏或争夺输入焦点。 */
		void configureCoordinatePopup(const HWND window)
		{
			if (!window)
			{
				return;
			}
			const auto style = GetWindowLongPtrW(window, GWL_EXSTYLE);
			SetWindowLongPtrW(window, GWL_EXSTYLE, (style | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW) & ~WS_EX_TRANSPARENT);
			SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
		}
	} // namespace

	ApplicationOptions parseApplicationOptions(const int argc, char *const *const argv)
	{
		ApplicationOptions options;
		for (int index = 1; index < argc; ++index)
		{
			const std::string_view argument(argv[index]);
			options.lifecycleHook |=
				argument == "--veloapp-install" || argument == "--veloapp-obsolete" || argument == "--veloapp-updated" || argument == "--veloapp-uninstall";
			options.preview |= argument == "--preview";
			options.diagnoseInput |= argument == "--diagnose-input";
		}
		return options;
	}

	int runApplication(const ApplicationOptions &options, const TraceSink &trace)
	{
		if (options.lifecycleHook)
		{
			return 0;
		}
		const bool preview		 = options.preview;
		const bool diagnoseInput = options.diagnoseInput;
		SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
		const auto directory	 = AppConfig::executableDirectory();
		const auto dataDirectory = preview ? directory / "preview" : directory;
		AppConfig  config(dataDirectory);
		trace("Opening configuration and instance lock");
		// 锁句柄必须存活到事件循环退出及配置保存完成。
		const platform::InstanceLock instanceLock(config.filePath());
		if (!instanceLock.acquired())
		{
			if (instanceLock.error() != ERROR_SHARING_VIOLATION)
			{
				platform::WindowIntegration::showError(nullptr, L"无法打开配置目录或单实例锁。\nCannot open configuration directory or instance lock.");
			}
			return instanceLock.error() == ERROR_SHARING_VIOLATION ? 0 : 1;
		}
		if (config.load() == AppConfig::LoadResult::IoError)
		{
			platform::WindowIntegration::showError(nullptr, L"无法读取配置，程序未启动。\nCannot read configuration. Please check file permissions.");
			return 1;
		}
		trace("Loading assets and creating Slint components");
		UiResources													   resources;
		platform::Assets											   assets;
		auto														   app = MainWindow::create();
		std::optional<slint::ComponentHandle<CoordinateCaptureWindow>> captureWindow;
		slint::Timer												   captureTimer;
		if (preview)
		{
			app->set_window_title("Flori Input Preview");
		}
		const auto &state		  = app->global<AppState>();
		const auto	bindResources = [&](const auto &target)
		{
			target.on_lookup([&](const slint::SharedString &key, const bool chinese)
							 { return slint::SharedString(resources.text(std::string_view(key.data(), key.size()), chinese)); });
			target.on_diagnostic([&](const slint::SharedString &source, const bool chinese)
								 { return slint::SharedString(resources.message(std::string_view(source.data(), source.size()), chinese)); });
			target.on_theme_color([&](const slint::SharedString &key, const bool dark)
								  { return slint::Color::from_argb_encoded(resources.color(std::string_view(key.data(), key.size()), dark)); });
			target.set_chinese(config.language() == "zh");
			target.set_dark(config.darkTheme());
		};
		bindResources(state);
		state.set_diagnose_input(diagnoseInput);
		state.on_input_trace([&](const slint::SharedString &message) { trace(std::string_view(message.data(), message.size())); });
		state.set_version(FLORI_INPUT_APP_VERSION);
		state.set_author(FLORI_INPUT_AUTHOR_NAME);
		bool										   previousInputActive = false, overlayVisible = false, closing = false;
		std::uint64_t								   overlayGeneration{};
		std::atomic<HWND>							   dispatchWindow{};
		std::unique_ptr<platform::WindowIntegration>   windows;
		std::function<void(const ClickerController &)> sync;
		const auto									   moveCoordinatePopup = [&](const POINT cursor)
		{
			if (!overlayVisible || !captureWindow)
			{
				return;
			}
			auto	  &window = (*captureWindow)->window();
			const HWND handle = window.win32_hwnd();
			if (!handle)
			{
				return;
			}
			const auto placement = coordinatePopupPlacement(cursor, monitorAtPoint(cursor), window.size());
			if ((*captureWindow)->get_info_on_right() != placement.infoOnRight)
			{
				(*captureWindow)->set_info_on_right(placement.infoOnRight);
			}
			if ((*captureWindow)->get_info_on_bottom() != placement.infoOnBottom)
			{
				(*captureWindow)->set_info_on_bottom(placement.infoOnBottom);
			}
			RECT current{};
			if (GetWindowRect(handle, &current) && current.left == placement.position.x && current.top == placement.position.y)
			{
				return;
			}
			SetWindowPos(handle, nullptr, placement.position.x, placement.position.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
		};
		const auto updateCoordinatePopup = [&]
		{
			if (!overlayVisible || !captureWindow)
			{
				return;
			}
			POINT cursor{};
			if (!GetCursorPos(&cursor))
			{
				return;
			}
			if ((*captureWindow)->get_screen_x() != cursor.x)
			{
				(*captureWindow)->set_screen_x(cursor.x);
			}
			if ((*captureWindow)->get_screen_y() != cursor.y)
			{
				(*captureWindow)->set_screen_y(cursor.y);
			}
			moveCoordinatePopup(cursor);
		};
		TaskScheduler	   execution;
		InputRouter		   router;
		bool			   pasteCapture = false, scriptCapture = false;
		bool			   scriptCoordinateCopied = false;
		std::optional<int> pendingPage;
		std::string		   pasteError, scriptError;
		bool			   scriptValidated = false;
		const auto		   scriptDirectory = config.filePath().parent_path().parent_path() / "scripts";
		auto			   scriptPath	   = [&]
		{
			const auto				   &name = config.eventScriptSettings().file;
			const std::filesystem::path relative(std::u8string(name.begin(), name.end()));
			if (name.empty() || relative.has_parent_path() || relative.filename() != relative)
			{
				throw InputError("[[script.selection_required]]");
			}
			const auto path = (scriptDirectory / relative).u8string();
			return std::string(reinterpret_cast<const char *>(path.data()), path.size());
		};
		// 选择或刷新时校验当前文件；启动前会再次读取并校验。
		const auto validateSelectedScript = [&]
		{
			scriptValidated = false;
			scriptError.clear();
			if (config.eventScriptSettings().file.empty())
			{
				return;
			}
			try
			{
				const auto program = platform::loadEventScript(scriptPath());
				validateScriptProgram(program, config.eventScriptSettings().limits);
				platform::validateVisibleMouseMoves(program);
				scriptValidated = true;
			}
			catch (const std::exception &error)
			{
				scriptError = error.what();
			}
		};
		auto refreshScripts = [&]
		{
			scriptValidated = false;
			try
			{
				const auto						 files = platform::listEventScripts(scriptDirectory);
				std::vector<slint::SharedString> rows;
				for (const auto &file: files)
				{
					rows.emplace_back(file);
				}
				state.set_script_files(std::make_shared<slint::VectorModel<slint::SharedString>>(rows));
				if (auto settings = config.eventScriptSettings(); std::ranges::find(files, settings.file) == files.end())
				{
					settings.file.clear();
					config.setEventScriptSettings(settings);
				}
				validateSelectedScript();
			}
			catch (const std::exception &error)
			{
				state.set_script_files(std::make_shared<slint::VectorModel<slint::SharedString>>());
				scriptError = error.what();
			}
		};
		std::function<void(ClickerController &, std::uint32_t)> routeKey;
		ClickerRuntime runtime([&](auto task) { platform::WindowIntegration::dispatchTask(dispatchWindow.load(), std::move(task)); },
							   [&](const auto &controller)
							   {
								   if (sync)
								   {
									   sync(controller);
								   }
							   },
							   [&](const std::uint32_t key)
							   {
								   DWORD foregroundProcess{};
								   GetWindowThreadProcessId(GetForegroundWindow(), &foregroundProcess);
								   trace("input key-down=" + std::to_string(key) + ", foreground-process=" + std::to_string(foregroundProcess)
										 + ", self=" + std::to_string(GetCurrentProcessId()));
							   },
							   [&](ClickerController &controller, const std::uint32_t key)
							   {
								   if (routeKey)
								   {
									   routeKey(controller, key);
								   }
							   },
							   [&](const InjectorConfig &input)
							   {
								   try
								   {
									   platform::NativeInputSender sender;
									   if (preview)
									   {
										   sender = [](const std::span<const INPUT> events) { return static_cast<UINT>(events.size()); };
									   }
									   const bool started = execution.start(Feature::Clicker, platform::makeWindowsTriggerTask(input, std::move(sender)));
									   if (!started)
									   {
										   state.set_error("[[execution.unavailable]]");
									   }
									   else
									   {
										   state.set_error("");
									   }
									   return started;
								   }
								   catch (const std::exception &error)
								   {
									   state.set_error(slint::SharedString(error.what()));
									   return false;
								   }
							   },
							   [&] { execution.stop(); },
							   [&](const ScreenPoint point) { moveCoordinatePopup({.x = point.x, .y = point.y}); });
		runtime.setCoordinateSelectionHandler(
			[&](const ScreenPoint point)
			{
				try
				{
					const auto coordinates = std::to_wstring(point.x) + L", " + std::to_wstring(point.y);
					platform::writeClipboardText(app->window().win32_hwnd(), coordinates);
					scriptCoordinateCopied = true;
					scriptError.clear();
				}
				catch (const std::exception &error)
				{
					scriptCoordinateCopied = false;
					scriptError			   = error.what();
				}
			});
		sync = [&](const ClickerController &controller)
		{
			if (diagnoseInput)
			{
				trace("runtime state: running=" + std::to_string(controller.running()) + ", capture=" + std::to_string(static_cast<int>(controller.capture())));
			}
			const bool chinese																								   = config.language() == "zh";
			const auto &[inputKey, action, cursorMode, customKey, lockedCoordinate, cycle, pressDuration, timingJitterPercent] = controller.config().injector;
			const bool wantsOverlay	   = controller.capture() == Capture::Coordinate || controller.capture() == Capture::CoordinateSelection;
			const bool changingOverlay = windows && wantsOverlay != overlayVisible;
			runtime.setCoordinateCaptureActive(wantsOverlay);
			state.set_chinese(chinese);
			state.set_dark(config.darkTheme());
			state.set_scale(static_cast<float>(config.uiScale()));
			state.set_theme_mode(config.settings().themeMode);
			state.set_language_mode(config.settings().languageMode);
			state.set_scale_index(config.settings().uiScaleIndex);
			const bool inputActive = controller.running() || execution.active();
			state.set_running(inputActive);
			state.set_can_start(controller.canStart());
			state.set_capture(static_cast<int>(controller.capture()));
			const auto &[hotkey, pressSeconds, intervalSeconds, unicode] = config.pasteSettings();
			state.set_paste_capture(pasteCapture);
			state.set_paste_hotkey(slint::SharedString(keyName(hotkey, chinese, resources)));
			state.set_paste_press(slint::SharedString(fixed(pressSeconds, 3)));
			state.set_paste_interval(slint::SharedString(fixed(intervalSeconds, 3)));
			state.set_paste_strategy(unicode ? 1 : 0);
			state.set_paste_error(slint::SharedString(pasteError));
			state.set_paste_status(pasteCapture				  ? (resources.text("status.capture_hotkey_hint", chinese))
									   : controller.running() ? (resources.text("status.trigger_busy", chinese))
									   : execution.state() == TaskScheduler::State::Stopping ? (resources.text("status.stopping", chinese))
									   : execution.active()									 ? (resources.text("status.stop_hint", chinese))
																							 : (resources.text("status.focus_target_hint", chinese)));
			state.set_input_key(static_cast<int>(inputKey));
			state.set_action(static_cast<int>(action));
			state.set_cursor(static_cast<int>(cursorMode));
			state.set_wheel(inputKey == InputKey::Custom && (customKey == WheelUpKey || customKey == WheelDownKey));
			state.set_start_hotkey(slint::SharedString(keyName(controller.config().startHotkey, chinese, resources)));
			state.set_custom_key(slint::SharedString(keyName(customKey, chinese, resources)));
			state.set_coordinate(slint::SharedString(std::to_string(lockedCoordinate.x) + "," + std::to_string(lockedCoordinate.y)));
			state.set_period(slint::SharedString(fixed(std::chrono::duration<double>(cycle).count(), 3)));
			state.set_press_duration(slint::SharedString(fixed(std::chrono::duration<double>(pressDuration).count(), 3)));
			state.set_jitter(slint::SharedString(fixed(timingJitterPercent, 0)));
			state.set_status(slint::SharedString(status(controller, chinese, resources)));
			if (execution.active() && execution.owner() != Feature::Clicker)
			{
				state.set_status(resources.text("status.tool_busy", chinese));
			}
			if (execution.state() == TaskScheduler::State::Stopping)
			{
				state.set_status(resources.text("status.stopping", chinese));
			}
			const auto &scriptSettings = config.eventScriptSettings();
			state.set_script_file(slint::SharedString(scriptSettings.file));
			state.set_script_hotkey(slint::SharedString(keyName(scriptSettings.hotkey, chinese, resources)));
			state.set_script_capture(scriptCapture);
			state.set_script_error(slint::SharedString(scriptError));
			const auto scriptStatus = [&]() -> const char *
			{
				if (scriptCapture)
				{
					return resources.text("status.capture_hotkey", chinese);
				}
				if (controller.capture() == Capture::CoordinateSelection)
				{
					return resources.text("coordinates.copy_hint", chinese);
				}
				if (scriptCoordinateCopied)
				{
					return resources.text("coordinates.copied", chinese);
				}
				if (execution.state() == TaskScheduler::State::Stopping)
				{
					return resources.text("status.stopping", chinese);
				}
				if (execution.owner() == Feature::Script)
				{
					return resources.text("script.stop_hint", chinese);
				}
				if (scriptSettings.file.empty())
				{
					return resources.text("script.selection_required", chinese);
				}
				return resources.text(scriptValidated ? "status.start_hint" : "script.validation_hint", chinese);
			}();
			state.set_script_status(scriptStatus);
			if (previousInputActive != inputActive && !preview)
			{
				assets.play(inputActive);
			}
			previousInputActive = inputActive;
			if (changingOverlay)
			{
				overlayVisible		  = wantsOverlay;
				const auto generation = ++overlayGeneration;
				if (wantsOverlay)
				{
					trace("Coordinate overlay show scheduled");
					slint::invoke_from_event_loop(
						[&, generation]
						{
							if (closing || generation != overlayGeneration || !overlayVisible)
							{
								return;
							}
							trace("Coordinate overlay create begin");
							if (!captureWindow)
							{
								captureWindow.emplace(CoordinateCaptureWindow::create());
								bindResources((*captureWindow)->global<AppState>());
								(*captureWindow)
									->window()
									.on_close_requested(
										[&]
										{
											runtime.update([](auto &controller) { controller.cancelCoordinateCapture(); });
											return slint::CloseRequestResponse::KeepWindowShown;
										});
								(*captureWindow)->window().set_size(slint::LogicalSize({.width = 240.0f, .height = 110.0f}));
							}
							POINT cursor{};
							if (!GetCursorPos(&cursor))
							{
								runtime.update([](auto &controller) { controller.cancelCoordinateCapture(); });
								return;
							}
							(*captureWindow)->set_screen_x(cursor.x);
							(*captureWindow)->set_screen_y(cursor.y);
							const auto placement = coordinatePopupPlacement(cursor, monitorAtPoint(cursor), (*captureWindow)->window().size());
							(*captureWindow)->set_info_on_right(placement.infoOnRight);
							(*captureWindow)->set_info_on_bottom(placement.infoOnBottom);
							(*captureWindow)->window().set_position(slint::PhysicalPosition({.x = placement.position.x, .y = placement.position.y}));
							configureCoordinatePopup((*captureWindow)->window().win32_hwnd());
							(*captureWindow)->show();
							configureCoordinatePopup((*captureWindow)->window().win32_hwnd());
							updateCoordinatePopup();
							captureTimer.start(slint::TimerMode::Repeated, std::chrono::milliseconds(16), updateCoordinatePopup);
							trace("Coordinate overlay create complete");
						});
				}
				else
				{
					trace("Coordinate overlay hide scheduled");
					captureTimer.stop();
					slint::invoke_from_event_loop(
						[&, generation]
						{
							if (generation != overlayGeneration || overlayVisible)
							{
								return;
							}
							if (captureWindow)
							{
								trace("Coordinate overlay hide begin");
								(*captureWindow)->hide();
								trace("Coordinate overlay hide complete");
								slint::invoke_from_event_loop(
									[&, generation]
									{
										if (generation != overlayGeneration || overlayVisible)
										{
											return;
										}
										trace("Coordinate overlay destroy begin");
										captureWindow.reset();
										trace("Coordinate overlay destroy complete");
									});
							}
						});
				}
			}
		};
		trace("Applying configuration");
		if (!config.applyTo(runtime))
		{
			platform::WindowIntegration::showError(nullptr, L"无法应用触发器配置。\nCannot apply trigger settings.");
			return 1;
		}
		routeKey = [&](ClickerController &controller, const std::uint32_t key)
		{
			if (closing || pendingPage)
			{
				return;
			}
			const auto feature = router.feature();
			const auto hotkey  = feature == Feature::Clicker ? controller.config().startHotkey
				 : feature == Feature::Paste				 ? config.pasteSettings().hotkey
															 : config.eventScriptSettings().hotkey;
			const auto capture = controller.capture() == Capture::CoordinateSelection ? Feature::Script
				: controller.capture() != Capture::None								  ? Feature::Clicker
				: pasteCapture														  ? Feature::Paste
				: scriptCapture														  ? Feature::Script
																					  : Feature::None;
			const auto action  = router.route(capture, execution.owner(), key, hotkey);
			if (action == InputRouter::Action::Ignore)
			{
				return;
			}
			if (action == InputRouter::Action::Capture)
			{
				if (controller.capture() != Capture::None)
				{
					controller.handleKeyPressed(key);
					return;
				}
				auto &error = feature == Feature::Paste ? pasteError : scriptError;
				if (key == VK_ESCAPE)
				{
					pasteCapture  = false;
					scriptCapture = false;
					error.clear();
				}
				else if (key >= VK_BACK && key <= 0xfe && key != VK_PACKET && keyMap.contains(key))
				{
					if (feature == Feature::Paste)
					{
						auto settings	= config.pasteSettings();
						settings.hotkey = key;
						config.setPasteSettings(settings);
					}
					else
					{
						auto settings	= config.eventScriptSettings();
						settings.hotkey = key;
						config.setEventScriptSettings(settings);
					}
					pasteCapture  = false;
					scriptCapture = false;
					error.clear();
				}
				else
				{
					error = resources.text("error.keyboard_key_required", config.language() == "zh");
				}
				return;
			}
			if (action == InputRouter::Action::Stop)
			{
				if (feature == Feature::Clicker)
				{
					controller.stop();
				}
				else
				{
					execution.stop();
				}
				return;
			}
			if (feature == Feature::Clicker)
			{
				controller.handleKeyPressed(key);
				return;
			}
			if (feature == Feature::Script)
			{
				scriptCoordinateCopied = false;
			}
			auto &error = feature == Feature::Paste ? pasteError : scriptError;
			error.clear();
			try
			{
				if (execution.blocked())
				{
					throw InputError("[[execution.cleanup_restart]]");
				}
				if (preview)
				{
					throw InputError("[[execution.preview_disabled]]");
				}
				const HWND target = GetForegroundWindow();
				DWORD	   targetProcess{};
				if (!target || !GetWindowThreadProcessId(target, &targetProcess) || targetProcess == GetCurrentProcessId())
				{
					throw InputError("[[paste.focus_required]]");
				}
				auto program = [&]
				{
					if (feature == Feature::Script)
					{
						return platform::loadEventScript(scriptPath());
					}
					const auto text = state.get_paste_text();
					const auto plain =
						text.size() ? plainTextProgramUtf8(std::string_view(text.data(), text.size())) : plainTextProgram(platform::clipboardTextSnapshot());
					const auto	&settings = config.pasteSettings();
					InputOptions input_options;
					input_options.strategy = settings.unicode ? TextInputStrategy::Unicode : TextInputStrategy::PreferPhysical;
					input_options.press	   = std::chrono::nanoseconds(static_cast<std::int64_t>(std::round(settings.pressSeconds * 1e9)));
					input_options.interval = std::chrono::nanoseconds(static_cast<std::int64_t>(std::round(settings.intervalSeconds * 1e9)));
					return withInputOptions(plain, input_options);
				}();
				const std::optional<ScriptExecutionLimits> scriptLimits =
					feature == Feature::Script ? std::optional<ScriptExecutionLimits>{config.eventScriptSettings().limits} : std::nullopt;
				if (feature == Feature::Script)
				{
					platform::validateVisibleMouseMoves(program);
				}
				auto task = makeProgramExecutionTask(std::move(program), platform::pasteBackendFactory(target, hotkey), scriptLimits);
				if (feature == Feature::Script)
				{
					scriptValidated = true;
				}
				if (!execution.start(feature, std::move(task)))
				{
					error = execution.error().empty() ? "[[execution.unavailable]]" : execution.error();
				}
			}
			catch (const std::exception &failure)
			{
				error = failure.what();
				if (feature == Feature::Script)
				{
					scriptValidated = false;
				}
			}
		};
		auto cancelCapture = [&]
		{
			runtime.invalidateKeyEvents();
			pasteCapture = scriptCapture = false;
			runtime.update([](auto &controller) { controller.beginCapture(Capture::None); });
		};
		state.on_request_page(
			[&](const int page)
			{
				cancelCapture();
				if (execution.active())
				{
					pendingPage = page;
					execution.stop();
					sync(runtime.controller());
				}
				else
				{
					app->set_page(page);
				}
			});
		state.on_select_page(
			[&](const int page)
			{
				cancelCapture();
				scriptCoordinateCopied = false;
				router.selectPage(page);
				if (router.feature() == Feature::Script)
				{
					refreshScripts();
				}
				if (execution.active())
				{
					execution.stop();
				}
				sync(runtime.controller());
			});
		state.on_script_begin_capture(
			[&]
			{
				if (execution.active() || router.feature() != Feature::Script)
				{
					return;
				}
				cancelCapture();
				scriptCoordinateCopied = false;
				scriptCapture		   = true;
				scriptError.clear();
				sync(runtime.controller());
			});
		state.on_script_pick_coordinate(
			[&]
			{
				if (execution.active() || router.feature() != Feature::Script)
				{
					return;
				}
				cancelCapture();
				scriptCoordinateCopied = false;
				scriptError.clear();
				runtime.update([](auto &controller) { controller.beginCapture(Capture::CoordinateSelection); });
			});
		state.on_script_select(
			[&](const slint::SharedString &name)
			{
				if (execution.active() || router.feature() != Feature::Script)
				{
					return;
				}
				cancelCapture();
				scriptCoordinateCopied = false;
				auto settings		   = config.eventScriptSettings();
				settings.file		   = std::string(name.data(), name.size());
				config.setEventScriptSettings(settings);
				validateSelectedScript();
				sync(runtime.controller());
			});
		state.on_script_file_action(
			[&](const int action)
			{
				if (execution.active() || router.feature() != Feature::Script)
				{
					return;
				}
				cancelCapture();
				scriptCoordinateCopied = false;
				scriptError.clear();
				try
				{
					if (action == 0)
					{
						std::filesystem::create_directories(scriptDirectory);
						platform::WindowIntegration::openLink(scriptDirectory.wstring());
					}
					else if (action == 1)
					{
						refreshScripts();
					}
					else if (action == 2)
					{
						platform::editEventScript(app->window().win32_hwnd(), scriptPath());
						scriptValidated = false;
					}
				}
				catch (const std::exception &error)
				{
					scriptError		= error.what();
					scriptValidated = false;
				}
				sync(runtime.controller());
			});
		state.on_paste_begin_capture(
			[&]
			{
				if (execution.active() || router.feature() != Feature::Paste)
				{
					return;
				}
				cancelCapture();
				pasteCapture = true;
				pasteError.clear();
				sync(runtime.controller());
			});
		state.on_paste_edit_strategy(
			[&](const int strategy)
			{
				if (runtime.controller().running() || execution.active() || strategy < 0 || strategy > 1)
				{
					return;
				}
				auto settings	 = config.pasteSettings();
				settings.unicode = strategy == 1;
				config.setPasteSettings(settings);
				pasteError.clear();
				sync(runtime.controller());
			});
		state.on_paste_edit_number(
			[&](const int field, const slint::SharedString &text)
			{
				if (runtime.controller().running() || execution.active() || field < 0 || field > 1)
				{
					return;
				}
				double value{};
				if (const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
					ec != std::errc{} || ptr != text.data() + text.size() || !std::isfinite(value) || value < 0 || value > 60)
				{
					return;
				}
				auto settings													= config.pasteSettings();
				(field == 0 ? settings.pressSeconds : settings.intervalSeconds) = value;
				config.setPasteSettings(settings);
				pasteError.clear();
				sync(runtime.controller());
			});
		slint::Timer pasteTimer;
		pasteTimer.start(slint::TimerMode::Repeated,
						 std::chrono::milliseconds(15),
						 [&]
						 {
							 const auto finished = execution.poll();
							 if (finished == Feature::None)
							 {
								 return;
							 }
							 if (finished == Feature::Clicker)
							 {
								 runtime.update([](auto &controller) { controller.executionFinished(); });
								 if (!execution.error().empty())
								 {
									 state.set_error(slint::SharedString(execution.error()));
								 }
							 }
							 else if (finished == Feature::Paste)
							 {
								 pasteError = execution.error();
							 }
							 else
							 {
								 scriptError = execution.error();
							 }
							 if (pendingPage)
							 {
								 const int page = *pendingPage;
								 pendingPage.reset();
								 if (!execution.blocked())
								 {
									 app->set_page(page);
								 }
								 else
								 {
									 state.set_system_error("[[execution.cleanup_restart]]");
								 }
							 }
							 sync(runtime.controller());
						 });
		auto editConfig = [&](const std::function<void(ClickerController::Config &)> &edit)
		{
			runtime.update(
				[&](auto &controller)
				{
					if (controller.running() || execution.active())
					{
						return;
					}
					auto next = controller.config();
					edit(next);
					controller.beginCapture(Capture::None);
					controller.setConfig(next);
				});
		};
		state.on_edit_option(
			[&](const int field, const int value)
			{
				trace("edit-option callback: field=" + std::to_string(field) + ", value=" + std::to_string(value));
				if (runtime.controller().running() || execution.active())
				{
					return;
				}
				if (field < 3)
				{
					editConfig(
						[&](auto &next)
						{
							if (field == 0)
							{
								next.injector.inputKey = static_cast<InputKey>(value);
							}
							if (field == 1)
							{
								next.injector.action = static_cast<InputAction>(value);
							}
							if (field == 2)
							{
								next.injector.cursorMode = static_cast<CursorMode>(value);
							}
						});
				}
				else
				{
					auto settings = config.settings();
					if (field == 3)
					{
						settings.themeMode = value;
					}
					if (field == 4)
					{
						settings.uiScaleIndex = value;
					}
					if (field == 5)
					{
						settings.languageMode = value;
					}
					config.setSettings(settings);
					sync(runtime.controller());
				}
			});
		state.on_edit_number(
			[&](const int field, const slint::SharedString &text)
			{
				if (field < 0 || field > 2)
				{
					return;
				}
				double value{};
				if (const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
					ec != std::errc{} || ptr != text.data() + text.size() || !std::isfinite(value))
				{
					return;
				}
				if ((field == 0 && (value < 0.001 || value > 99999)) || (field == 1 && (value < 0 || value > MaximumTimingJitterPercent))
					|| (field == 2 && (value < 0 || value > 99999)))
				{
					return;
				}
				editConfig(
					[&](auto &next)
					{
						if (field == 0)
						{
							next.injector.cycle = std::chrono::nanoseconds(static_cast<std::int64_t>(std::round(value * 1e9)));
						}
						else if (field == 1)
						{
							next.injector.timingJitterPercent = value;
						}
						else if (field == 2)
						{
							next.injector.pressDuration = std::chrono::nanoseconds(static_cast<std::int64_t>(std::round(value * 1e9)));
						}
					});
			});
		state.on_begin_capture(
			[&](const int kind)
			{
				if (execution.active() || router.feature() != Feature::Clicker)
				{
					return;
				}
				cancelCapture();
				runtime.update([&](auto &controller) { controller.beginCapture(static_cast<Capture>(kind)); });
			});
		state.on_open_repository([] { platform::WindowIntegration::openLink(L"https://github.com/flowersauce/Flori-Input"); });
		state.on_open_manual(
			[&]
			{
				platform::WindowIntegration::openLink(config.language() == "zh" ? L"https://flowersauce.github.io/Flori-Input/"
																					  : L"https://flowersauce.github.io/Flori-Input/en/");
			});
		state.on_open_slint([] { platform::WindowIntegration::openLink(L"https://slint.dev"); });
		state.on_open_licenses(
			[&]
			{
				platform::WindowIntegration::openLink(config.language() == "zh" ? L"https://flowersauce.github.io/Flori-Input/legal/third-party/"
																					  : L"https://flowersauce.github.io/Flori-Input/en/legal/third-party/");
			});
		auto close = [&]
		{
			if (closing)
			{
				return;
			}
			trace("Shutting down runtime");
			++overlayGeneration;
			captureTimer.stop();
			if (captureWindow)
			{
				(*captureWindow)->hide();
			}
			pasteTimer.stop();
			runtime.shutdown();
			execution.shutdown();
			dispatchWindow = nullptr;
			trace("Saving configuration");
			if (!config.save(runtime))
			{
				platform::WindowIntegration::showError(app->window().win32_hwnd(),
													   L"配置保存失败，原配置未覆盖。\nConfiguration could not be saved. The previous file was preserved.");
			}
			trace("Closing windows and event loop");
			closing = true;
			app->hide();
			slint::quit_event_loop();
		};
		app->on_quit_requested(close);
		app->window().on_close_requested(
			[&]
			{
				close();
				return slint::CloseRequestResponse::KeepWindowShown;
			});
		app->on_minimize_requested(
			[&]
			{
				trace("minimize callback");
				app->window().set_minimized(true);
			});
		trace("Showing main window");
		app->show();
		// show() 可能早于原生窗口创建；等待界面事件循环后再挂接 Win32 子类，避免阻塞窗口创建。
		std::exception_ptr startupError;
		slint::Timer	   windowTimer;
		const auto		   windowDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
		windowTimer.start(slint::TimerMode::Repeated,
						  std::chrono::milliseconds(10),
						  [&]
						  {
							  if (closing)
							  {
								  windowTimer.stop();
								  return;
							  }
							  try
							  {
								  const auto handle = app->window().win32_hwnd();
								  if (!handle)
								  {
									  if (std::chrono::steady_clock::now() < windowDeadline)
									  {
										  return;
									  }
									  throw std::runtime_error("Main window HWND was not created within 2 seconds");
								  }
								  windowTimer.stop();
								  trace("Installing native window integration");
								  windows = std::make_unique<platform::WindowIntegration>(
									  handle,
									  [&]
									  {
										  config.refreshSystemPreferences();
										  updateCoordinatePopup();
										  sync(runtime.controller());
									  },
									  runtime.keyCallback(),
									  diagnoseInput ? trace : std::function<void(std::string_view)>{},
									  [&](const bool active) { state.set_window_active(active); });
								  dispatchWindow = handle;
								  if (!preview && !runtime.installHooks())
								  {
									  state.set_system_error(resources.text("error.hotkey_hook_failed", config.language() == "zh"));
								  }
								  sync(runtime.controller());
								  trace("Window ready; running=" + std::to_string(state.get_running()));
							  }
							  catch (...)
							  {
								  // 不得让 C++ 异常穿过 Slint 事件循环回调。
								  startupError = std::current_exception();
								  windowTimer.stop();
								  slint::quit_event_loop();
							  }
						  });
		trace("Entering event loop");
		slint::run_event_loop();
		windowTimer.stop();
		if (startupError)
		{
			std::rethrow_exception(startupError);
		}
		if (!closing)
		{
			close();
		}
		windows.reset();
		trace("Event loop exited");
		return 0;
	}
} // namespace flori_input
