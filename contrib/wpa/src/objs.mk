$(_OBJS_VAR) := $(call BUILDOBJ,$($(_OBJS_VAR)))
-include $(filter-out %.a,$($(_OBJS_VAR):%.o=%.d))
_DIRS += $(dir $($(_OBJS_VAR)))
