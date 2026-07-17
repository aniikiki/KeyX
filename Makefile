#---------------------------------------------------------------------------------
# KeyX 主 Makefile
# 用于编译 ovl 和 sys 模块，并将编译产物复制到 out 目录
#---------------------------------------------------------------------------------

.PHONY: all clean ovl sys show-result zip

# 从 ovl-KeyX/Makefile 读取版本号
VERSION := $(shell awk '/^[[:space:]]*APP_VERSION[[:space:]]*:=/ { print $$3; exit }' ovl-KeyX/Makefile)
PYTHON ?= python3

# 模块目录
OVL_DIR := ovl-KeyX
SYS_DIR := sys-KeyX

# 输出目录
OUT_DIR := out
OUT_EN := $(OUT_DIR)/EN
OUT_CN := $(OUT_DIR)/CN

# EN 版本路径（英文标题）
OUT_EN_SWITCH := $(OUT_EN)/switch
OUT_EN_OVERLAYS := $(OUT_EN_SWITCH)/.overlays
OUT_EN_ATMOSPHERE := $(OUT_EN)/atmosphere/contents/4100000002025924

# CN 版本路径（中文标题）
OUT_CN_SWITCH := $(OUT_CN)/switch
OUT_CN_OVERLAYS := $(OUT_CN_SWITCH)/.overlays
OUT_CN_ATMOSPHERE := $(OUT_CN)/atmosphere/contents/4100000002025924

# 编译产物路径
OVL_OUTPUT := $(OVL_DIR)/ovl-KeyX.ovl
SYS_OUTPUT_DIR := $(SYS_DIR)/out/4100000002025924

# 状态文件
BUILD_STATUS := .build_status

# 颜色定义
COLOR_GREEN := \033[0;32m
COLOR_RED := \033[0;31m
COLOR_RESET := \033[0m

#---------------------------------------------------------------------------------
# 默认目标：编译所有模块并复制到 out 目录
#---------------------------------------------------------------------------------
all:
	@rm -f $(BUILD_STATUS)
	@$(MAKE) --no-print-directory ovl sys || true
	@$(MAKE) --no-print-directory show-result

#---------------------------------------------------------------------------------
# 显示最终编译结果
#---------------------------------------------------------------------------------
show-result:
	@echo "========================================"
	@if [ -f $(BUILD_STATUS) ]; then \
		if grep -q "编译失败" $(BUILD_STATUS); then \
			echo "编译结果："; \
			while IFS= read -r line; do \
				if echo "$$line" | grep -q "编译失败"; then \
					printf "$(COLOR_RED)%s$(COLOR_RESET)\n" "$$line"; \
				elif echo "$$line" | grep -q "编译完成"; then \
					printf "$(COLOR_GREEN)%s$(COLOR_RESET)\n" "$$line"; \
				fi; \
			done < $(BUILD_STATUS); \
			rm -f $(BUILD_STATUS); \
			exit 1; \
		else \
			rm -rf $(OUT_EN) $(OUT_CN); \
			rm -f $(OUT_DIR)/KeyX-$(VERSION)-CN.zip \
				$(OUT_DIR)/KeyX-$(VERSION)-EN.zip \
				$(OUT_DIR)/KeyX-CN.zip \
				$(OUT_DIR)/KeyX-EN.zip \
				"$(OUT_DIR)/按键助手（连发宏映射）-$(VERSION)-XX.zip"; \
			mkdir -p $(OUT_EN_OVERLAYS) $(OUT_EN_ATMOSPHERE); \
			mkdir -p $(OUT_CN_OVERLAYS) $(OUT_CN_ATMOSPHERE); \
			cp -f $(OVL_OUTPUT) $(OUT_EN_OVERLAYS)/; \
			cp -rf $(SYS_OUTPUT_DIR)/* $(OUT_EN_ATMOSPHERE)/; \
			cp -f $(OVL_OUTPUT) $(OUT_CN_OVERLAYS)/; \
			cp -rf $(SYS_OUTPUT_DIR)/* $(OUT_CN_ATMOSPHERE)/; \
			echo "编译结果："; \
			if $(PYTHON) $(OVL_DIR)/ChineseTitle.py $(OUT_CN_OVERLAYS)/ovl-KeyX.ovl > /dev/null 2>&1; then \
				printf "$(COLOR_GREEN)  模块 ovl-KeyX，改名成功$(COLOR_RESET)\n"; \
			else \
				printf "$(COLOR_RED)  模块 ovl-KeyX，改名失败$(COLOR_RESET)\n"; \
				rm -f $(BUILD_STATUS); \
				exit 1; \
			fi; \
			(cd $(OUT_CN) && zip -rq ../KeyX-$(VERSION)-CN.zip .); \
			(cd $(OUT_EN) && zip -rq ../KeyX-$(VERSION)-EN.zip .); \
			cp $(OUT_DIR)/KeyX-$(VERSION)-CN.zip "$(OUT_DIR)/按键助手（连发宏映射）-$(VERSION)-XX.zip"; \
			cp $(OUT_DIR)/KeyX-$(VERSION)-CN.zip "$(OUT_DIR)/KeyX-CN.zip"; \
			cp $(OUT_DIR)/KeyX-$(VERSION)-EN.zip "$(OUT_DIR)/KeyX-EN.zip"; \
			printf "$(COLOR_GREEN)  模块 ovl-KeyX，编译完成，已打包ZIP$(COLOR_RESET)\n"; \
			printf "$(COLOR_GREEN)  模块 sys-KeyX，编译完成，已打包ZIP$(COLOR_RESET)\n"; \
			echo "========================================"; \
			echo "输出目录："; \
			printf "$(COLOR_GREEN)     $(OUT_DIR)/$(COLOR_RESET)\n"; \
			echo "========================================"; \
			rm -f $(BUILD_STATUS); \
		fi; \
	fi

#---------------------------------------------------------------------------------
# 编译 overlay 模块111
#---------------------------------------------------------------------------------
ovl:
	@echo "========================================"
	@echo "编译 overlay 模块..."
	@echo "========================================"
	@if $(MAKE) --no-print-directory -C $(OVL_DIR); then \
		echo "  模块 ovl-KeyX，编译完成" >> $(BUILD_STATUS); \
	else \
		echo "  模块 ovl-KeyX，编译失败" >> $(BUILD_STATUS); \
		exit 1; \
	fi

#---------------------------------------------------------------------------------
# 编译 sysmodule 模块
#---------------------------------------------------------------------------------
sys:
	@echo "========================================"
	@echo "编译 sysmodule 模块..."
	@echo "========================================"
	@if $(MAKE) --no-print-directory -C $(SYS_DIR); then \
		echo "  模块 sys-KeyX，编译完成" >> $(BUILD_STATUS); \
	else \
		echo "  模块 sys-KeyX，编译失败" >> $(BUILD_STATUS); \
		exit 1; \
	fi

#---------------------------------------------------------------------------------
# 清理所有编译产物
#---------------------------------------------------------------------------------
clean:
	@echo "========================================"
	@echo "清理所有编译产物..."
	@echo "========================================"
	@echo "清理 overlay 模块..."
	@$(MAKE) --no-print-directory -C $(OVL_DIR) clean
	@echo "清理 sysmodule 模块..."
	@$(MAKE) --no-print-directory -C $(SYS_DIR) clean
	@echo "清理 out 目录..."
	@rm -rf $(OUT_DIR)
	@rm -f $(BUILD_STATUS)
	@echo "========================================"
	@echo "清理结果："
	@printf "$(COLOR_GREEN)  模块 ovl-KeyX， 已清理$(COLOR_RESET)\n"
	@printf "$(COLOR_GREEN)  模块 sys-KeyX， 已清理$(COLOR_RESET)\n"
	@printf "$(COLOR_GREEN)  输出 out/            ，已删除$(COLOR_RESET)\n"
	@echo "========================================"

#---------------------------------------------------------------------------------
# 打包 CN 和 EN 版本为 ZIP
#---------------------------------------------------------------------------------
zip: all
	@echo "========================================"
	@echo "打包 ZIP 文件 (v$(VERSION))..."
	@echo "========================================"
	@cd $(OUT_CN) && zip -rq ../KeyX-$(VERSION)-CN.zip .
	@printf "$(COLOR_GREEN)  已生成: $(OUT_DIR)/KeyX-$(VERSION)-CN.zip$(COLOR_RESET)\n"
	@cd $(OUT_EN) && zip -rq ../KeyX-$(VERSION)-EN.zip .
	@printf "$(COLOR_GREEN)  已生成: $(OUT_DIR)/KeyX-$(VERSION)-EN.zip$(COLOR_RESET)\n"
	@echo "========================================"
