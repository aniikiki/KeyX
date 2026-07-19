#---------------------------------------------------------------------------------
# KeyX 主 Makefile
# 用于编译 ovl 和 sys 模块，并生成简体中文安装包
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
PACKAGE_DIR := $(OUT_DIR)/package
PACKAGE_OVERLAYS := $(PACKAGE_DIR)/switch/.overlays
PACKAGE_ATMOSPHERE := $(PACKAGE_DIR)/atmosphere/contents/4100000002025924
PACKAGE_ZIP := $(OUT_DIR)/KeyX-$(VERSION)-CN.zip

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
			rm -rf $(OUT_DIR); \
			mkdir -p $(PACKAGE_OVERLAYS) $(PACKAGE_ATMOSPHERE); \
			cp -f $(OVL_OUTPUT) $(PACKAGE_OVERLAYS)/; \
			cp -rf $(SYS_OUTPUT_DIR)/* $(PACKAGE_ATMOSPHERE)/; \
			echo "编译结果："; \
			if $(PYTHON) $(OVL_DIR)/ChineseTitle.py $(PACKAGE_OVERLAYS)/ovl-KeyX.ovl > /dev/null 2>&1; then \
				printf "$(COLOR_GREEN)  模块 ovl-KeyX，改名成功$(COLOR_RESET)\n"; \
			else \
				printf "$(COLOR_RED)  模块 ovl-KeyX，改名失败$(COLOR_RESET)\n"; \
				rm -f $(BUILD_STATUS); \
				exit 1; \
			fi; \
			(cd $(PACKAGE_DIR) && zip -rq ../KeyX-$(VERSION)-CN.zip .); \
			rm -rf $(PACKAGE_DIR); \
			printf "$(COLOR_GREEN)  模块 ovl-KeyX，编译完成，已打包ZIP$(COLOR_RESET)\n"; \
			printf "$(COLOR_GREEN)  模块 sys-KeyX，编译完成，已打包ZIP$(COLOR_RESET)\n"; \
			echo "========================================"; \
			echo "输出目录："; \
			printf "$(COLOR_GREEN)     $(PACKAGE_ZIP)$(COLOR_RESET)\n"; \
			echo "========================================"; \
			rm -f $(BUILD_STATUS); \
		fi; \
	fi

#---------------------------------------------------------------------------------
# 编译 overlay 模块
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
# 编译并打包简体中文版本
#---------------------------------------------------------------------------------
zip: all
