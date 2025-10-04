# SPDX-License-Identifier: Apache-2.0
#
# Copyright (c) 2024, Centro de Inovacao EDGE

if (NOT DEFINED BOARD_REVISION)
    set(BOARD_REVISION ${LIST_BOARD_REVISION_DEFAULT})
    message(DEBUG "Revisão não especificada, compilando para ${BOARD}@${BOARD_REVISION}.")
elseif (NOT BOARD_REVISION IN_LIST LIST_BOARD_REVISIONS)
    message(FATAL_ERROR "Revisão ${BOARD_REVISION} para ${BOARD} não encontrada. Verifique e tente novamente.\
                         Revisões válidas: ${LIST_BOARD_REVISIONS}")
endif ()
